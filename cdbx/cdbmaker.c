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
#define FL_DESTROY   (1 << 1)
#define FL_CLOSED    (1 << 2)
#define FL_COMMITTED (1 << 3)
#define FL_ERROR     (1 << 4)
#define FL_FP_CLOSE  (1 << 5)
#define FL_MMAP_SET  (1 << 6)
#define FL_MMAP_VAL  (1 << 7)

/*
 * Object structure for CDBMakerType
 */
typedef struct {
    PyObject_HEAD
    PyObject *weakreflist;

    cdbx_cdb32_maker_t *maker32;

    PyObject *cdb_cls;
    PyObject *fp;
    PyObject *filename;
    int flags;
} cdbmaker_t;

static PyObject *
CDBMakerType_close(cdbmaker_t *);

/* -------------------------- BEGIN CDBMakerType ------------------------- */

PyDoc_STRVAR(CDBMakerType_commit__doc__,
"commit(self)\n\
\n\
Commit to the current dataset and finish the CDB creation.\n\
\n\
The `commit` method returns a new CDB instance based on the file just\n\
committed.\n\
\n\
Returns:\n\
  CDB: New CDB instance");

static PyObject *
CDBMakerType_commit(cdbmaker_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {NULL};
    PyObject *result, *tmp;
    int close = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist))
        return NULL;

    if (self->flags & (FL_CLOSED | FL_COMMITTED | FL_ERROR))
        return cdbx_raise_closed();

    if (-1 == cdbx_cdb32_maker_commit(self->maker32)) {
        self->flags |= FL_ERROR;
        return NULL;
    }
    self->flags |= FL_COMMITTED;

    if (-1 == fsync(cdbx_cdb32_maker_fileno(self->maker32))) {
        /* LCOV_EXCL_START */

        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;

        /* LCOV_EXCL_STOP */
    }

    tmp = (self->flags & FL_MMAP_SET) ?
        ((self->flags & FL_MMAP_VAL) ? Py_True : Py_False) : Py_None;

    if (self->filename) {
        result = PyObject_CallFunction(self->cdb_cls, "(OiO)",
                                       self->filename, 1, tmp);
        close = 1;
    }
    else if (self->fp) {
        result = PyObject_CallFunction(self->cdb_cls, "(OiO)", self->fp,
                                       !!(self->flags & FL_FP_CLOSE), tmp);
    }
    else {
        result = PyObject_CallFunction(self->cdb_cls, "(iiO)",
                                       cdbx_cdb32_maker_fileno(self->maker32),
                                       !!(self->flags & FL_FP_CLOSE), tmp);
    }
    if (!result)
        LCOV_EXCL_LINE_RETURN(NULL);

    self->flags &= ~FL_DESTROY;
    if (close)
        self->flags |= FL_FP_CLOSE;
    else
        self->flags &= ~FL_FP_CLOSE;

    if (!(tmp = CDBMakerType_close(self))) {
        /* LCOV_EXCL_START */

        Py_DECREF(result);
        return NULL;

        /* LCOV_EXCL_STOP */
    }
    Py_DECREF(tmp);

    return result;
}


PyDoc_STRVAR(CDBMakerType_add__doc__,
"add(self, key, value)\n\
\n\
Add the key/value pair to the CDB-to-be.\n\
\n\
Note that in case of a unicode key or value, it will be transformed to a\n\
byte string using the latin-1 encoding.\n\
\n\
Parameters:\n\
  key (str or bytes)\n\
    Key\n\
\n\
  value (str or bytes):\n\
    Value");

static PyObject *
CDBMakerType_add(cdbmaker_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"key", "value", NULL};
    PyObject *key_, *value_;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist,
                                     &key_, &value_))
        return NULL;

    if (self->flags & (FL_CLOSED | FL_COMMITTED | FL_ERROR))
        return cdbx_raise_closed();

    if (-1 == cdbx_cdb32_maker_add(self->maker32, key_, value_)) {
        self->flags |= FL_ERROR;
        return NULL;
    }

    Py_RETURN_NONE;
}


PyDoc_STRVAR(CDBMakerType_close__doc__,
"close(self)\n\
\n\
Close the CDBMaker and destroy the file (if it was created by the maker or\n\
explicitly requested in the constructor)");

static PyObject *
CDBMakerType_close(cdbmaker_t *self)
{
    PyObject *fp, *fname, *result;
    int res = 0, fd = -1;

    self->flags |= FL_CLOSED;

    if (self->maker32) {
        fd = cdbx_cdb32_maker_fileno(self->maker32);
        cdbx_cdb32_maker_destroy(&self->maker32);
    }

    if ((fp = self->fp)) {
        self->fp = NULL;
        if (self->flags & (FL_FP_OPENED | FL_FP_CLOSE)) {
            if (!(result = PyObject_CallMethod(fp, "close", ""))) {
                res = -1;  /* LCOV_EXCL_LINE */
            }
            else {
                Py_DECREF(result);
                if ((self->flags & FL_DESTROY) && (fname = self->filename)) {
                    self->filename = NULL;
                    res = cdbx_unlink(fname);
                    Py_DECREF(fname);
                }
            }
        }
        Py_DECREF(fp);
    }
    else if (fd >= 0 && (self->flags & FL_FP_CLOSE)) {
        if ((close(fd) < 0) && errno != EINTR) {
            PyErr_SetFromErrno(PyExc_OSError);
            res = -1;
        }
        else {
            res = 0;
        }
    }

    if (res == -1)
        LCOV_EXCL_LINE_RETURN(NULL);
    Py_RETURN_NONE;
}


PyDoc_STRVAR(CDBMakerType_fileno__doc__,
"fileno(self)\n\
\n\
Find underlying file descriptor\n\
\n\
Returns:\n\
  int: The file descriptor");

#ifdef EXT3
#define PyInt_FromLong PyLong_FromLong
#endif

static PyObject *
CDBMakerType_fileno(cdbmaker_t *self)
{
    if (self->flags & FL_CLOSED)
        return cdbx_raise_closed();

    return PyInt_FromLong(cdbx_cdb32_maker_fileno(self->maker32));
}

#ifdef EXT3
#undef PyInt_FromLong
#endif


static PyMethodDef CDBMakerType_methods[] = {
    {"close",
     EXT_CFUNC(CDBMakerType_close),           METH_NOARGS,
     CDBMakerType_close__doc__},

    {"fileno",
     EXT_CFUNC(CDBMakerType_fileno),          METH_NOARGS,
     CDBMakerType_fileno__doc__},

    {"add",
    EXT_CFUNC(CDBMakerType_add),              METH_KEYWORDS | METH_VARARGS,
     CDBMakerType_add__doc__},

    {"commit",
     EXT_CFUNC(CDBMakerType_commit),          METH_KEYWORDS | METH_VARARGS,
     CDBMakerType_commit__doc__},

    /* Sentinel */
    {NULL, NULL}
};


static int
CDBMakerType_traverse(cdbmaker_t *self, visitproc visit, void *arg)
{
    Py_VISIT(self->fp);
    Py_VISIT(self->filename);
    Py_VISIT(self->cdb_cls);

    return 0;
}


static int
CDBMakerType_clear(cdbmaker_t *self)
{
    PyObject *result;

    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (!(result = CDBMakerType_close(self))) {
        PyErr_Clear();  /* LCOV_EXCL_LINE */
    }
    else {
        Py_DECREF(result);
    }

    Py_CLEAR(self->filename);
    Py_CLEAR(self->cdb_cls);

    return 0;
}


DEFINE_GENERIC_DEALLOC(CDBMakerType)


PyDoc_STRVAR(CDBMakerType__doc__,
"CDBMaker - use CDB.make to create instance");

EXT_LOCAL PyTypeObject CDBMakerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    EXT_MODULE_PATH ".CDBMaker",                        /* tp_name */
    sizeof(cdbmaker_t),                                 /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor)CDBMakerType_dealloc,                   /* tp_dealloc */
    0,                                                  /* tp_print */
    0,                                                  /* tp_getattr */
    0,                                                  /* tp_setattr */
    0,                                                  /* tp_compare */
    0,                                                  /* tp_repr */
    0,                                                  /* tp_as_number */
    0,                                                  /* tp_as_sequence */
    0,                                                  /* tp_as_mapping */
    (hashfunc)PyObject_HashNotImplemented,              /* tp_hash */
    0,                                                  /* tp_call */
    0,                                                  /* tp_str */
    PyObject_GenericGetAttr,                            /* tp_getattro */
    0,                                                  /* tp_setattro */
    0,                                                  /* tp_as_buffer */
    Py_TPFLAGS_HAVE_WEAKREFS                            /* tp_flags */
    | Py_TPFLAGS_HAVE_CLASS
    | Py_TPFLAGS_HAVE_GC,
    CDBMakerType__doc__,                                /* tp_doc */
    (traverseproc)CDBMakerType_traverse,                /* tp_traverse */
    (inquiry)CDBMakerType_clear,                        /* tp_clear */
    0,                                                  /* tp_richcompare */
    offsetof(cdbmaker_t, weakreflist),                  /* tp_weaklistoffset */
    0,                                                  /* tp_iter */
    0,                                                  /* tp_iternext */
    CDBMakerType_methods,                               /* tp_methods */
};


/*
 * Create new CDBMaker object
 */
EXT_LOCAL PyObject *
cdbx_maker_new(PyTypeObject *cdb_cls, PyObject *file_, PyObject *close_,
               PyObject *mmap_)
{
    cdbmaker_t *self;
    int fd, res;

    if (!(self = GENERIC_ALLOC(&CDBMakerType)))
        LCOV_EXCL_LINE_RETURN(NULL);

    self->maker32 = NULL;
    self->flags = FL_CLOSED | FL_DESTROY;
    self->cdb_cls = (PyObject *)cdb_cls;
    Py_INCREF(self->cdb_cls);

    if (-1 == cdbx_obj_as_fd(file_, "w+b", &self->filename, &self->fp,
                             &res, &fd))
        goto error;
    if (res)
        self->flags |= FL_FP_OPENED;
    self->flags &= ~FL_CLOSED;

    if (close_) {
        switch (PyObject_IsTrue(close_)) {
        case -1: goto error;
        case 1: self->flags |= FL_FP_CLOSE;
        }
    }

    if (mmap_ && mmap_ != Py_None) {
        switch (PyObject_IsTrue(mmap_)) {
        case -1: goto error;
        case 1: self->flags |= FL_MMAP_VAL; /* fall through */
        case 0: self->flags |= FL_MMAP_SET; break;
        }
    }

    if (-1 == cdbx_cdb32_maker_create(fd, &self->maker32))
        LCOV_EXCL_LINE_GOTO(error);

    return (PyObject *)self;

error:
    Py_DECREF(self);
    return NULL;
}

/* --------------------------- END CDBMakerType -------------------------- */
