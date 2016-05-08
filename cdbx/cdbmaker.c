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
#include <unistd.h>
#include <stdlib.h>

/*
 * Object structure for CDBType
 */
typedef struct {
    PyObject_HEAD
    PyObject *weakreflist;

    struct cdb_make maker;
    PyObject *fp;
    PyObject *filename;
    int fp_opened;
    int destroy;
    int closed;
} cdbmaker_t;

static int
CDBMakerType_clear(cdbmaker_t *);

/* ------------------------ BEGIN Helper Functions ----------------------- */

/* ------------------------- END Helper Functions ------------------------ */

/* -------------------------- BEGIN CDBMakerType ------------------------- */

PyDoc_STRVAR(CDBMakerType_commit__doc__,
"commit()\n\
\n\
Commit to the current dataset and finish the CDB creation.\n\
\n\
:Parameters:\n\
  `key` : ``str``\n\
    Key\n\
\n\
  `value` : ``value``\n\
    Value");

static PyObject *
CDBMakerType_commit(cdbmaker_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist
                                     ))
        return NULL;

    if (self->closed)
        return cdbx_raise_closed();

    if (-1 == cdb_make_finish(&self->maker)
        || -1 == fsync(self->maker.fd)) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    self->destroy = 0;
    if (-1 == CDBMakerType_clear(self))
        return NULL;

    Py_RETURN_NONE;
}

PyDoc_STRVAR(CDBMakerType_add__doc__,
"add(key, value)\n\
\n\
Add the key/value pair to the CDB-to-be.\n\
\n\
:Parameters:\n\
  `key` : ``str``\n\
    Key\n\
\n\
  `value` : ``value``\n\
    Value");

static PyObject *
CDBMakerType_add(cdbmaker_t *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"key", "value", NULL};
    PyObject *key_, *value_;
    char *ckey, *cvalue;
    cdb_len_t ksize, vsize;
    int res;

    if (self->closed)
        return cdbx_raise_closed();

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist,
                                     &key_, &value_))
        return NULL;

    if (-1 == cdbx_byte_key(&key_, &ckey, &ksize))
        return NULL;

    if (-1 == cdbx_byte_key(&value_, &cvalue, &vsize)) {
        Py_DECREF(key_);
        return NULL;
    }

    res = cdb_make_add(&self->maker, ckey, ksize, cvalue, vsize);
    Py_DECREF(value_);
    Py_DECREF(key_);

    if (res == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(CDBMakerType_close__doc__,
"CDBMaker.close() -> close maker and destroy");

static PyObject *
CDBMakerType_close(cdbmaker_t *self)
{
    if (-1 == CDBMakerType_clear(self))
        return NULL;

    Py_RETURN_NONE;
}

PyDoc_STRVAR(CDBMakerType_fileno__doc__,
"CDBMaker.fileno() -> int representing the underlying file descriptor");

#ifdef EXT3
#define PyInt_FromLong PyLong_FromLong
#endif

static PyObject *
CDBMakerType_fileno(cdbmaker_t *self)
{
    if (self->closed)
        return cdbx_raise_closed();

    return PyInt_FromLong(self->maker.fd);
}

#ifdef EXT3
#undef PyInt_FromLong
#endif

static PyMethodDef CDBMakerType_methods[] = {
    {"close",
     (PyCFunction)CDBMakerType_close,           METH_NOARGS,
     CDBMakerType_close__doc__},

    {"fileno",
     (PyCFunction)CDBMakerType_fileno,          METH_NOARGS,
     CDBMakerType_fileno__doc__},

    {"add",
    (PyCFunction)CDBMakerType_add,              METH_KEYWORDS | METH_VARARGS,
     CDBMakerType_add__doc__},

    {"commit",
     (PyCFunction)CDBMakerType_commit,          METH_KEYWORDS | METH_VARARGS,
     CDBMakerType_commit__doc__},

    /* Sentinel */
    {NULL, NULL}
};

static int
CDBMakerType_traverse(cdbmaker_t *self, visitproc visit, void *arg)
{
    Py_VISIT(self->fp);

    return 0;
}

static int
CDBMakerType_clear(cdbmaker_t *self)
{
    PyObject *fp, *fname;
    void *ptr;
    int res = 0;

    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    self->closed = 1;

    if ((ptr = self->maker.split)) {
        self->maker.split = NULL;
        free(ptr);
    }
    while ((ptr = self->maker.head)) {
        self->maker.head = self->maker.head->next;
        free(ptr);
    }

    if ((fp = self->fp)) {
        self->fp = NULL;
        if (self->fp_opened) {
            if (!PyObject_CallMethod(fp, "close", "")) {
                if (PyErr_ExceptionMatches(PyExc_Exception))
                    PyErr_Clear();
                else
                    res = -1;
            }
            if (self->destroy && (fname = self->filename)) {
                self->filename = NULL;
                if (-1 == cdbx_unlink(fname))
                    res = -1;
                Py_DECREF(fname);
            }
        }
        Py_DECREF(fp);
    }
    Py_CLEAR(self->filename);

    return res;
}

DEFINE_GENERIC_DEALLOC(CDBMakerType)

PyDoc_STRVAR(CDBMakerType__doc__,
"CDBMaker(cdb_class, file)");

PyTypeObject CDBMakerType = {
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
PyObject *
cdbx_maker_new(PyTypeObject *cdb_cls, PyObject *file_)
{
    cdbmaker_t *self;
    int fd;

    if (!(self = GENERIC_ALLOC(&CDBMakerType)))
        return NULL;

    self->maker.head = NULL;
    self->maker.split = NULL;
    self->closed = 1;
    self->destroy = 1;
    if (-1 == cdbx_obj_as_fd(file_, "w+b", &self->filename, &self->fp,
                             &self->fp_opened, &fd))
        goto error;

    self->closed = 0;
    if (-1 == cdb_make_start(&self->maker, fd))
        goto error;

    return (PyObject *)self;

error:
    Py_DECREF(self);
    return NULL;
}

/* --------------------------- END CDBMakerType -------------------------- */
