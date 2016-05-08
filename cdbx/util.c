/*
 * Copyright 2016
 * Andr\xe9 Malo or his licensors, as applicable
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cdbx.h"

/*
 * Unlink file
 *
 * Return -1 on error
 * Return 0 on success
 */
int
cdbx_unlink(PyObject *filename)
{
    PyObject *os, *result;
    int res;

    if (!(os = PyImport_ImportModule("os")))
        return -1;

    result = PyObject_CallMethod(os, "unlink", "(O)", filename);
    res = result ? 0 : -1;
    Py_DECREF(result);

    Py_DECREF(os);
    return res;
}

/*
 * Turn filename into a full path'd filename
 *
 * Return NULL on error
 */
static PyObject *
full_filename(PyObject *filename)
{
    PyObject *os_path, *tmp, *result;

    if (!(os_path = PyImport_ImportModule("os.path")))
        return NULL;

    if (!(tmp = PyObject_CallMethod(os_path, "abspath", "(O)", filename)))
        goto error;

    result = PyObject_CallMethod(os_path, "normpath", "(O)", tmp);
    Py_DECREF(tmp);

    Py_DECREF(os_path);
    return result;

error:
    Py_DECREF(os_path);
    return NULL;
}

/*
 * Turn python object into a file descriptor
 *
 * Return -1 on error
 * Return 0 on success
 */
int
cdbx_obj_as_fd(PyObject *file_, char *mode, PyObject **fname_,
               PyObject **fp_, int *opened, int *fd_)
{
    PyObject *fileno, *tmp, *filename = NULL;
    int res;

    *fp_ = NULL;
    *opened = 0;

    Py_INCREF(file_);

    /* try fileno(), that would work on open files */
    if (-1 == cdbx_attr(file_, "fileno", &fileno))
        goto error;
    if (fileno) {
        tmp = PyObject_CallFunction(fileno, "");
        Py_DECREF(fileno);
        if (!tmp)
            goto error;

        res = cdbx_fd(tmp, fd_);
        Py_DECREF(tmp);
        if (res == -1)
            goto error;

        *opened = 0;
        *fp_ = file_;
        if (fname_)
            *fname_ = NULL;
    }

    /* int (a.k.a FD) */
    else if (
#ifdef EXT2
            PyInt_Check(file_) ||
#endif
            PyLong_Check(file_))
    {
        res = cdbx_fd(file_, fd_);
        Py_DECREF(file_);
        if (res == -1)
            return -1;

        *opened = 0;
        *fp_ = NULL;
        if (fname_)
            *fname_ = NULL;
    }

    /* filename */
    else {
        if (!(filename = full_filename(file_)))
            goto error;
        Py_DECREF(file_);
        file_ = filename;
        if (!(tmp = cdbx_file_open(file_, mode)))
            goto error;
        file_ = tmp;

        if (!(tmp = PyObject_CallMethod(file_, "fileno", "()")))
            goto error_fname;

        res = cdbx_fd(tmp, fd_);
        Py_DECREF(tmp);
        if (res == -1)
            goto error_fname;

        *opened = 1;
        *fp_ = file_;
        if (fname_)
            *fname_ = filename;
    }

    return 0;

error_fname:
    Py_DECREF(filename);
error:
    Py_DECREF(file_);
    return -1;
}

/*
 * Transform bytes/unicode key to char/len
 *
 * RC(key_) is increased on success. However, *key_ might be replaced (Py2).
 *
 * Return -1 on error
 * Return 0 on success
 */
int
cdbx_byte_key(PyObject **key_, char **ckey, cdb32_len_t *csize)
{
    Py_ssize_t length;
    PyObject *key = *key_;

    Py_INCREF(key);
    if (PyBytes_Check(key)) {
        if (-1 == PyBytes_AsStringAndSize(key, ckey, &length))
            goto error;
    }
    else if (PyUnicode_Check(key)) {
#ifdef EXT2
        PyObject *tmp;
        if (!(tmp = PyUnicode_AsUTF8String(key)))
            goto error;

        Py_DECREF(key);
        *key_ = key = tmp;
        if (-1 == PyBytes_AsStringAndSize(key, ckey, &length))
            goto error;
#else
        if (!(*ckey = PyUnicode_AsUTF8AndSize(key, &length)))
            goto error;
#endif
    }
    else {
        PyErr_SetString(PyExc_TypeError,
#ifdef EXT2
        "Key must be a unicode or str object"
#else
        "Key must be a str or bytes object"
#endif
        );
        goto error;
    }

    /* should not happen. But what do I know? */
    *csize = (cdb32_len_t)length;
    if ((Py_ssize_t)*csize != length) {
        PyErr_SetString(PyExc_OverflowError, "Key is too long");
        goto error;
    }

    return 0;

error:
    Py_DECREF(key);
    return -1;
}


/*
 * set IOError("I/O operation on a closed file") and return NULL
 */
PyObject *
cdbx_raise_closed(void)
{
    PyErr_SetString(PyExc_IOError, "I/O operation on a closed file");
    return NULL;
}


#ifdef EXT3
/*
 * Open a file
 *
 * Return NULL on error
 */
PyObject *
cdbx_file_open(PyObject *filename, const char *mode)
{
   PyObject *io, *result;

    if (!(io = PyImport_ImportModule("io")))
        return NULL;
    result = PyObject_CallMethod(io, "open", "(Os)", filename, mode);
    Py_DECREF(io);

    return result;
}
#endif


/*
 * Convert int object to fd
 *
 * Return -1 on error
 * Return 0 if no error occured.
 */
int
cdbx_fd(PyObject *obj, int *fd)
{
    long long_int;

#ifdef EXT2
    if (PyInt_Check(obj)) {
        long_int = PyInt_AsLong(obj);
    }
    else {
#endif
        long_int = PyLong_AsLong(obj);
#ifdef EXT2
    }
#endif
    if (PyErr_Occurred())
        return -1;
    if (long_int > (long)INT_MAX || long_int < 0L) {
        PyErr_SetNone(PyExc_OverflowError);
        return -1;
    }

    *fd = (int)long_int;
    return 0;
}


/*
 * Find a particular pyobject attribute
 *
 * Return -1 on error
 * Return 0 if no error occured. attribute will be NULL if it was simply not
 * found.
 */
int
cdbx_attr(PyObject *obj, const char *name, PyObject **attr)
{
    PyObject *result;

    if ((result = PyObject_GetAttrString(obj, name))) {
        *attr = result;
        return 0;
    }
    else if (!PyErr_Occurred()) {
        *attr = NULL;
        return 0;
    }
    else if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
        PyErr_Clear();
        *attr = NULL;
        return 0;
    }

    return -1;
}
