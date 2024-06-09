/*
 * Copyright 2016 - 2024
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

#define FL_FP_OPENED (1 << 0)

/*
 * Object structure for CDBType
 */
struct cdbtype_t {
    PyObject_HEAD
    PyObject *weakreflist;

    cdbx_cdb32_t *cdb32;  /* cdb struct, 32bit */

    PyObject *fp;  /* open file, might be NULL if fd was passed */
    int flags;
};

static int
CDBType_clear(cdbtype_t *);


/*
 * Return cdb32 struct member
 */
EXT_LOCAL cdbx_cdb32_t *
cdbx_type_get_cdb32(cdbtype_t *self)
{
    return self->cdb32;
}

/* ------------------------ BEGIN Helper Functions ----------------------- */

/*
 * Set KeyError(key)
 */
static void
raise_key_error(PyObject *key)
{
    PyObject *tmp;

    if ((tmp = PyTuple_Pack(1, key))) {
        PyErr_SetObject(PyExc_KeyError, tmp);
        Py_DECREF(tmp);
    }
}

/* ------------------------- END Helper Functions ------------------------ */

/* ---------------------------- BEGIN CDBType ---------------------------- */

static Py_ssize_t
CDBType_len_ssize_t(cdbtype_t *self)
{
    Py_ssize_t result;

    if (!self->cdb32) {
        cdbx_raise_closed();
        return -1;
    }

    if (-1 == cdbx_cdb32_count_keys(self->cdb32, &result))
        LCOV_EXCL_LINE_RETURN(-1);

    return result;
}


#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_len__doc__,
"__len__(self)\n\
\n\
Count the number of unique keys\n\
\n\
Returns:\n\
  int: The number of unique keys");

#ifdef EXT3
#define PyInt_FromSsize_t PyLong_FromSsize_t
#endif

static PyObject *
CDBType_len(cdbtype_t *self)
{
    Py_ssize_t result;

    if (-1 == (result = CDBType_len_ssize_t(self)))
        LCOV_EXCL_LINE_RETURN(NULL);

    return PyInt_FromSsize_t(result);
}

#ifdef EXT3
#undef PyInt_FromSsize_t
#endif

#endif


PyDoc_STRVAR(CDBType_get__doc__,
"get(self, key, default=None, all=False)\n\
\n\
Return value(s) for a key\n\
\n\
If `key` is not found, `default` is returned. If `key` was found, then\n\
depending on the `all` flag the value return is either a byte string\n\
(`all` == False) or a list of byte strings (`all` == True).\n\
\n\
Note that in case of a unicode key, it will be transformed to a byte string\n\
using the latin-1 encoding.\n\
\n\
Parameters:\n\
  key (bytes):\n\
    Key to lookup\n\
\n\
  default:\n\
    Default value to pass back if the key was not found\n\
\n\
  all (bool):\n\
    Return all values instead of only the first? Default: False\n\
\n\
Returns:\n\
  The value(s) or the default");

static PyObject *
CDBType_get(cdbtype_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"key", "default", "all", NULL};
    PyObject *key_, *default_ = NULL, *all_ = NULL;
    PyObject *result, *result_list = NULL;
    cdbx_cdb32_get_iter_t *get_iter;
    int res, all = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OO", kwlist,
                                     &key_, &default_, &all_))
        return NULL;

    if (!self->cdb32)
        return cdbx_raise_closed();

    if (default_)
        Py_INCREF(default_);
    else {
        Py_INCREF(Py_None);
        default_ = Py_None;
    }

    if (all_) {
        switch (PyObject_IsTrue(all_)) {
        case -1: goto error;
        case 1: all = 1;
        }
    }
    if (all && !(result_list = PyList_New(0)))
        LCOV_EXCL_LINE_GOTO(error);

    if (-1 == cdbx_cdb32_get_iter_new(self->cdb32, key_, &get_iter))
        LCOV_EXCL_LINE_GOTO(error_list);

    do {
        if (-1 == cdbx_cdb32_get_iter_next(get_iter, &result))
            LCOV_EXCL_LINE_GOTO(error_get_iter);

        if (all && result) {
            res = PyList_Append(result_list, result);
            Py_DECREF(result);
            if (-1 == res)
                LCOV_EXCL_LINE_GOTO(error_get_iter);
        }
    } while (all && result);
    cdbx_cdb32_get_iter_destroy(&get_iter);

    if (all) {
        if (!PyList_GET_SIZE(result_list)) {
            Py_DECREF(result_list);
            return default_;
        }
        Py_DECREF(default_);
        return result_list;
    }

    if (!result)
        return default_;

    Py_DECREF(default_);
    return result;

    /* LCOV_EXCL_START */
error_get_iter:
    cdbx_cdb32_get_iter_destroy(&get_iter);
error_list:
    if (all)
        Py_DECREF(result_list);
error:
    /* LCOV_EXCL_STOP */

    Py_DECREF(default_);
    return NULL;
}


PyDoc_STRVAR(CDBType_items__doc__,
"items(self, all=False)\n\
\n\
Create key/value pair iterator\n\
\n\
Parameters:\n\
  all (bool):\n\
    Return all (i.e. non-unique-key) items? Default: False\n\
\n\
Returns:\n\
  iterable: Iterator over items");

static PyObject *
CDBType_items(cdbtype_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"all", NULL};
    PyObject *all_ = NULL;
    int all = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist,
                                     &all_))
        return NULL;

    if (!self->cdb32)
        return cdbx_raise_closed();

    if (all_) {
        switch (PyObject_IsTrue(all_)) {
        case -1: return NULL;
        case 1: all = 1;
        }
    }

    return cdbx_iter_new(self, 1, all);
}


PyDoc_STRVAR(CDBType_keys__doc__,
"keys(self, all=False)\n\
\n\
Create key iterator\n\
\n\
Parameters:\n\
  all (bool):\n\
    Return all (i.e. non-unique) keys? Default: False\n\
\n\
Returns:\n\
  iterable: Iterator over keys");

static PyObject *
CDBType_keys(cdbtype_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"all", NULL};
    PyObject *all_ = NULL;
    int all = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist,
                                     &all_))
        return NULL;

    if (!self->cdb32)
        return cdbx_raise_closed();

    if (all_) {
        switch (PyObject_IsTrue(all_)) {
        case -1: return NULL;
        case 1: all = 1;
        }
    }

    return cdbx_iter_new(self, 0, all);
}


#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_iter__doc__,
"__iter__(self)\n\
\n\
Create an iterator over unique keys - in insertion order\n\
\n\
Returns:\n\
  iterable: The key iterator");
#endif

static PyObject *
CDBType_iter(cdbtype_t *self)
{
    if (!self->cdb32)
        return cdbx_raise_closed();

    return cdbx_iter_new(self, 0, 0);
}


PyDoc_STRVAR(CDBType_make__doc__,
"make(cls, file, close=None, mmap=None)\n\
\n\
Create a CDB maker instance, which returns a CDB instance when done.\n\
\n\
Parameters:\n\
  file (file or str or int):\n\
    Either a (binary) python stream (providing fileno()) or a filename or an\n\
    integer (representing a filedescriptor).\n\
\n\
  close (bool):\n\
    Close a passed in file automatically? This argument is only applied if\n\
    `file` is a python stream or an integer. If omitted or ``None`` it\n\
    defaults to ``False``. This argument is applied on commit.\n\
\n\
  mmap (bool):\n\
    Access the file by mapping it into memory? If True, mmap is required. If\n\
    false, mmap is not even tried. If omitted or ``None``, it's attempted but\n\
    no error on failure. This argument is applied on commit.\n\
\n\
Returns:\n\
  CDBMaker: New maker instance");

static PyObject *
CDBType_make(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"file", "close", "mmap", NULL};
    PyObject *file_, *close_ = NULL, *mmap_ = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OO", kwlist,
                                     &file_, &close_, &mmap_))
        return NULL;

    return cdbx_maker_new(cls, file_, close_, mmap_);
}


PyDoc_STRVAR(CDBType_close__doc__,
"close(self)\n\
\n\
Close the CDB.");

static PyObject *
CDBType_close(cdbtype_t *self)
{
    PyObject *fp, *result;
    int fd = -1;

    if (self->cdb32) {
        fd = cdbx_cdb32_fileno(self->cdb32);
        cdbx_cdb32_destroy(&self->cdb32);
    }

    if ((fp = self->fp)) {
        self->fp = NULL;

        if (self->flags & FL_FP_OPENED) {
            if (!(result = PyObject_CallMethod(fp, "close", ""))) {
                /* LCOV_EXCL_START */
                Py_DECREF(fp);
                return NULL;
                /* LCOV_EXCL_STOP */
            }
            Py_DECREF(result);
        }
        Py_DECREF(fp);
    }
    else if (fd >= 0 && (self->flags & FL_FP_OPENED)) {
        if ((close(fd) < 0) && errno != EINTR)
            return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}


PyDoc_STRVAR(CDBType_fileno__doc__,
"fileno(self)\n\
\n\
Find the underlying file descriptor\n\
\n\
Returns:\n\
  int: The underlying file descriptor");

#ifdef EXT3
#define PyInt_FromLong PyLong_FromLong
#endif

static PyObject *
CDBType_fileno(cdbtype_t *self)
{
    if (!self->cdb32)
        return cdbx_raise_closed();

    return PyInt_FromLong(cdbx_cdb32_fileno(self->cdb32));
}

#ifdef EXT3
#undef PyInt_FromLong
#endif


static int
CDBType_contains_int(cdbtype_t *self, PyObject *key)
{
    if (!self->cdb32) {
        cdbx_raise_closed();
        return -1;
    }

    return cdbx_cdb32_contains(self->cdb32, key);
}

PyDoc_STRVAR(CDBType_has_key__doc__,
"has_key(self, key)\n\
\n\
Check if the key appears in the CDB.\n\
\n\
Note that in case of a unicode key, it will be transformed to a byte string\n\
using the latin-1 encoding.\n\
\n\
Parameters:\n\
  key (str or bytes):\n\
    Key to look up\n\
\n\
Returns:\n\
  bool: Does the key exist?");

#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_contains__doc__,
"__contains__(self, key)\n\
\n\
Check if the key appears in the CDB.\n\
\n\
Note that in case of a unicode key, it will be transformed to a byte string\n\
using the latin-1 encoding.\n\
\n\
Parameters:\n\
  key (str or bytes):\n\
    Key to look up\n\
\n\
Returns:\n\
  bool: Does the key exist?");
#endif

static PyObject *
CDBType_contains(cdbtype_t *self, PyObject *key)
{
    switch (CDBType_contains_int(self, key)) {
    case -1: return NULL;
    case 0: Py_RETURN_FALSE;
    default: Py_RETURN_TRUE;
    }
}


#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_getitem__doc__,
"__getitem__(self, key)\n\
\n\
Find the first value of the passed key and return the value as bytestring\n\
\n\
Note that in case of a unicode key, it will be transformed to a byte string\n\
using the latin-1 encoding.\n\
\n\
Parameters:\n\
  key (str or bytes):\n\
    Key to look up\n\
\n\
Returns:\n\
  bytes: The first value of the key\n\
\n\
Raises:\n\
  KeyError: Key not found");
#endif

static PyObject *
CDBType_getitem(cdbtype_t *self, PyObject *key)
{
    PyObject *result;
    cdbx_cdb32_get_iter_t *get_iter;
    int res;

    if (!self->cdb32)
        return cdbx_raise_closed();

    if (-1 == cdbx_cdb32_get_iter_new(self->cdb32, key, &get_iter))
        return NULL;

    res = cdbx_cdb32_get_iter_next(get_iter, &result);
    cdbx_cdb32_get_iter_destroy(&get_iter);
    if (-1 == res)
        LCOV_EXCL_LINE_RETURN(NULL);

    if (!result)
        raise_key_error(key);

    return result;
}


static PySequenceMethods CDBType_as_sequence = {
    0,                                    /* sq_length */
    0,                                    /* sq_concat */
    0,                                    /* sq_repeat */
    0,                                    /* sq_item */
    0,                                    /* sq_slice */
    0,                                    /* sq_ass_item */
    0,                                    /* sq_ass_slice */
    (objobjproc)CDBType_contains_int,     /* sq_contains */
    0,                                    /* sq_inplace_concat */
    0                                     /* sq_inplace_repeat */
};

static PyMappingMethods CDBType_as_mapping = {
    (lenfunc)CDBType_len_ssize_t,    /* mp_length */
    (binaryfunc)CDBType_getitem,     /* mp_subscript */
    0                                /* mp_ass_subscript */
};


#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_new__doc__,
"__new__(cls, file, close=None, mmap=None)\n\
\n\
Create a CDB instance.\n\
\n\
Parameters:\n\
  file (file or str or int):\n\
    Either a (binary) python stream (providing fileno()) or a filename or an\n\
    integer (representing a filedescriptor).\n\
\n\
  close (bool):\n\
    Close a passed in file automatically? This argument is only applied if\n\
    `file` is a python stream or an integer. If omitted or ``None`` it\n\
    defaults to ``False``.\n\
\n\
  mmap (bool):\n\
    Access the file by mapping it into memory? If True, mmap is required. If\n\
    false, mmap is not even tried. If omitted or ``None``, it's attempted but\n\
    no error on failure.\n\
\n\
Returns:\n\
  CDB: New CDB instance");

static PyObject *
CDBType_new(PyTypeObject *cls, PyObject *args, PyObject *kwds);
#endif


static PyMethodDef CDBType_methods[] = {
#ifdef METH_COEXIST
    {"__new__",
     EXT_CFUNC(CDBType_new),                  METH_COEXIST  |
                                              METH_STATIC   |
                                              METH_KEYWORDS |
                                              METH_VARARGS,
     CDBType_new__doc__},

    {"__len__",
     EXT_CFUNC(CDBType_len),                  METH_NOARGS | METH_COEXIST,
     CDBType_len__doc__},

    {"__iter__",
     EXT_CFUNC(CDBType_iter),                 METH_NOARGS | METH_COEXIST,
     CDBType_iter__doc__},

    {"__contains__",
     EXT_CFUNC(CDBType_contains),             METH_O | METH_COEXIST,
     CDBType_contains__doc__},

    {"__getitem__",
     EXT_CFUNC(CDBType_getitem),              METH_O | METH_COEXIST,
     CDBType_getitem__doc__},
#endif

    {"make",
     EXT_CFUNC(CDBType_make),                 METH_CLASS    |
                                              METH_KEYWORDS |
                                              METH_VARARGS,
     CDBType_make__doc__},

    {"close",
     EXT_CFUNC(CDBType_close),                METH_NOARGS,
     CDBType_close__doc__},

    {"fileno",
     EXT_CFUNC(CDBType_fileno),               METH_NOARGS,
     CDBType_fileno__doc__},

    {"has_key",
     EXT_CFUNC(CDBType_contains),             METH_O,
     CDBType_has_key__doc__},

    {"keys",
     EXT_CFUNC(CDBType_keys),                 METH_KEYWORDS |
                                              METH_VARARGS,
     CDBType_keys__doc__},

    {"items",
     EXT_CFUNC(CDBType_items),                METH_KEYWORDS |
                                              METH_VARARGS,
     CDBType_items__doc__},

    {"get",
     EXT_CFUNC(CDBType_get),                  METH_KEYWORDS |
                                              METH_VARARGS,
     CDBType_get__doc__},

#if 0
    {"getiter",
     EXT_CFUNC(CDBType_getiter),              METH_VARARGS,
     CDBType_getiter__doc__},

    {"streamget",
     EXT_CFUNC(CDBType_get),                  METH_VARARGS,
     CDBType_get__doc__},

    {"streamgetiter",
     EXT_CFUNC(CDBType_getiter),              METH_VARARGS,
     CDBType_getiter__doc__},

    {"streamitems",
     EXT_CFUNC(CDBType_streamitems),          METH_NOARGS,
     CDBType_streamitems__doc__},
#endif

    /* Sentinel */
    {NULL, NULL}
};

static PyObject *
CDBType_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"file", "close", "mmap", NULL};
    PyObject *file_, *close_ = NULL, *mmap_ = NULL;
    cdbtype_t *self;
    int fd, res, mmap = -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OO", kwlist,
                                     &file_, &close_, &mmap_))
        return NULL;

    if (!(self = GENERIC_ALLOC(type)))
        LCOV_EXCL_LINE_RETURN(NULL);

    self->cdb32 = NULL;
    self->flags = 0;

    if (-1 == cdbx_obj_as_fd(file_, "rb", NULL, &self->fp, &res, &fd))
        goto error;
    if (res)
        self->flags |= FL_FP_OPENED;

    if (close_) {
        switch (PyObject_IsTrue(close_)) {
        case -1: goto error;
        case 1: self->flags |= FL_FP_OPENED;
        }
    }

    if (mmap_ && mmap_ != Py_None) {
        switch (PyObject_IsTrue(mmap_)) {
        case -1: goto error;
        case 0: mmap = 0; break;
        case 1: mmap = 1; break;
        }
    }

    if (-1 == cdbx_cdb32_create(fd, &self->cdb32, mmap))
        LCOV_EXCL_LINE_GOTO(error);

    return (PyObject *)self;

error:
    Py_DECREF(self);
    return NULL;
}


static int
CDBType_traverse(cdbtype_t *self, visitproc visit, void *arg)
{
    Py_VISIT(self->fp);

    return 0;
}


static int
CDBType_clear(cdbtype_t *self)
{
    PyObject *result;

    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (!(result = CDBType_close(self))) {
        PyErr_Clear();  /* LCOV_EXCL_LINE */
    }
    else {
        Py_DECREF(result);
    }

    return 0;
}


DEFINE_GENERIC_DEALLOC(CDBType)


PyDoc_STRVAR(CDBType__doc__,
"CDB(file, close=None, mmap=None)\n\
\n\
Create a CDB instance from a file.\n\
\n\
Parameters:\n\
  file (file or str or int):\n\
    Either a (binary) python stream (providing fileno()) or a filename or an\n\
    integer (representing a filedescriptor).\n\
\n\
  close (bool):\n\
    Close a passed in file automatically? This argument is only applied if\n\
    `file` is a python stream or an integer. If omitted or ``None`` it\n\
    defaults to ``False``.\n\
\n\
  mmap (bool):\n\
    Access the file by mapping it into memory? If True, mmap is required. If\n\
    false, mmap is not even tried. If omitted or ``None``, it's attempted but\n\
    no error on failure.");

EXT_LOCAL PyTypeObject CDBType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    EXT_MODULE_PATH ".CDB",                             /* tp_name */
    sizeof(cdbtype_t),                                  /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor)CDBType_dealloc,                        /* tp_dealloc */
    0,                                                  /* tp_print */
    0,                                                  /* tp_getattr */
    0,                                                  /* tp_setattr */
    0,                                                  /* tp_compare */
    0,                                                  /* tp_repr */
    0,                                                  /* tp_as_number */
    &CDBType_as_sequence,                               /* tp_as_sequence */
    &CDBType_as_mapping,                                /* tp_as_mapping */
    (hashfunc)PyObject_HashNotImplemented,              /* tp_hash */
    0,                                                  /* tp_call */
    0,                                                  /* tp_str */
    PyObject_GenericGetAttr,                            /* tp_getattro */
    0,                                                  /* tp_setattro */
    0,                                                  /* tp_as_buffer */
    Py_TPFLAGS_HAVE_WEAKREFS                            /* tp_flags */
    | Py_TPFLAGS_HAVE_CLASS
    | Py_TPFLAGS_HAVE_SEQUENCE_IN
    | Py_TPFLAGS_HAVE_ITER
    | Py_TPFLAGS_BASETYPE
    | Py_TPFLAGS_HAVE_GC,
    CDBType__doc__,                                     /* tp_doc */
    (traverseproc)CDBType_traverse,                     /* tp_traverse */
    (inquiry)CDBType_clear,                             /* tp_clear */
    0,                                                  /* tp_richcompare */
    offsetof(cdbtype_t, weakreflist),                   /* tp_weaklistoffset */
    (getiterfunc)CDBType_iter,                          /* tp_iter */
    0,                                                  /* tp_iternext */
    CDBType_methods,                                    /* tp_methods */
    0,                                                  /* tp_members */
    0,                                                  /* tp_getset */
    0,                                                  /* tp_base */
    0,                                                  /* tp_dict */
    0,                                                  /* tp_descr_get */
    0,                                                  /* tp_descr_set */
    0,                                                  /* tp_dictoffset */
    0,                                                  /* tp_init */
    0,                                                  /* tp_alloc */
    (newfunc)CDBType_new,                               /* tp_new */
};

/* ----------------------------- END CDBType ----------------------------- */
