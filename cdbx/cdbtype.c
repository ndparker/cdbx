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
 * Object structure for CDBType
 */
struct cdbtype_t {
    PyObject_HEAD
    PyObject *weakreflist;

    cdb32_t *cdb32;  /* cdb struct (original, 32 bit) */
    cdb32_ref_t eod; /* sentinel for iterating - cached value */
    cdb32_ref_t begin; /* start position for iterating - cached value */

    Py_ssize_t length;  /* Number of distinct keys - cached value */
    Py_ssize_t records;  /* Number of records (keys can be non-unique) - cached */
    PyObject *fp;  /* open file, might be NULL if fd was passed */
    int fp_opened;
    int pos_cached;
};

static int
CDBType_clear(cdbtype_t *);


/*
 * Return cdb32 struct member
 */
cdb32_t *
cdbx_get_cdb32(cdbtype_t *self)
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

#define CDB32_READ_NUM(pos) do {                                     \
    if (-1 == cdb32_read(self->cdb32, buf, CDB32_NUM_SIZE, (pos))) { \
        PyErr_SetFromErrno(PyExc_IOError);                           \
        goto error;                                                  \
    }                                                                \
    (pos) += CDB32_NUM_SIZE;                                         \
} while (0)

static Py_ssize_t
CDBType_length(cdbtype_t *self)
{
    char buf[CDB32_NUM_SIZE];
    void *kbuf = NULL, *kbuftmp;
    size_t ksize;
    Py_ssize_t result, records;
    cdb32_ref_t eod, klen, dlen, pos;

    if (!self->cdb32) {
        cdbx_raise_closed();
        return -1;
    }

    /* We need to count it, so count it and cache */
    if (self->length < 0) {
        /* 1st: init pointers */
        if (!self->pos_cached) {
            pos = 0;
            CDB32_READ_NUM(pos);
            cdb32_num_unpack(buf, &eod);
            self->eod = eod;
            while (pos < CDB32_TABLE_SIZE) CDB32_READ_NUM(pos);
            self->begin = pos;
            self->pos_cached = 1;
        }
        else {
            eod = self->eod;
            pos = self->begin;
        }

        result = 0;
        records = 0;
        ksize = 0;
        kbuf = NULL;

        /* And now count the keys */
        while (pos < eod) {
            if (!(records < PY_SSIZE_T_MAX)) {
                PyErr_SetString(PyExc_OverflowError, "Number of keys too big");
                return -1;
            }
            CDB32_READ_NUM(pos);
            cdb32_num_unpack(buf, &klen);
            CDB32_READ_NUM(pos);
            cdb32_num_unpack(buf, &dlen);
            if (!kbuf || ksize < (size_t)klen) {
                if (!(kbuftmp = PyMem_Realloc(kbuf, (size_t)klen))) {
                    PyErr_NoMemory();
                    goto error;
                }
                kbuf = kbuftmp;
                ksize = (size_t)klen;
            }

            if (-1 == cdb32_read(self->cdb32, kbuf, klen, pos)) {
                PyErr_SetFromErrno(PyExc_IOError);
                goto error;
            }
            pos += klen;

            switch (cdb32_find(self->cdb32, kbuf, klen)) {
            case -1:
                PyErr_SetFromErrno(PyExc_IOError);
                goto error;

            case 0:
                PyErr_SetString(PyExc_ValueError, "CDB Format Error");
                goto error;

            default:
                ++records;
                if (cdb32_datapos(self->cdb32) == pos)
                    ++result;
            }
            pos += dlen;
        }

        if (kbuf)
            PyMem_Free(kbuf);

        self->length = result;
        self->records = records;
    }

    return self->length;

error:
    if (kbuf)
        PyMem_Free(kbuf);
    return -1;
}

#undef CDB32_READ_NUM

PyDoc_STRVAR(CDBType_keys__doc__,
"CDB.keys() - return iterator over unique keys");
#define CDBType_keys CDBType_iter

#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_iter__doc__,
"CDB.__iter__() - return iterator over unique keys");
#endif

static PyObject *
CDBType_iter(cdbtype_t *self)
{
    if (!self->cdb32)
        return cdbx_raise_closed();

    return cdbx_keyiter_new(self);
}

PyDoc_STRVAR(CDBType_make__doc__,
"make(cls, file)\n\
\n\
Create a CDB maker instance, which returns a CDB instance when done.\n\
\n\
:Parameters:\n\
  `file` : ``file`` or ``str`` or ``fd``\n\
    Either a (binary) python stream (providing fileno()) or a filename or an\n\
    integer (representing a filedescriptor).\n\
\n\
:Return: New maker instance\n\
:Rtype: ``CDBMaker``");

static PyObject *
CDBType_make(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"file", NULL};
    PyObject *file_;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
                                     &file_))
        return NULL;

    return cdbx_maker_new(cls, file_);
}

PyDoc_STRVAR(CDBType_close__doc__,
"CDB.close() - close the cdb");

static PyObject *
CDBType_close(cdbtype_t *self)
{
    CDBType_clear(self);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(CDBType_fileno__doc__,
"CDB.fileno() -> int representing the underlying file descriptor");

#ifdef EXT3
#define PyInt_FromLong PyLong_FromLong
#endif

static PyObject *
CDBType_fileno(cdbtype_t *self)
{
    if (!self->cdb32)
        return cdbx_raise_closed();

    return PyInt_FromLong(self->cdb32->fd);
}

#ifdef EXT3
#undef PyInt_FromLong
#endif

static int
CDBType_contains_int(cdbtype_t *self, PyObject *key)
{
    char *ckey;
    cdb32_len_t csize;
    int res;

    if (!self->cdb32) {
        cdbx_raise_closed();
        return -1;
    }

    if (-1 == cdbx_byte_key(&key, &ckey, &csize))
        return -1;

    res = cdb32_find(self->cdb32, ckey, csize);
    Py_DECREF(key);
    if (res == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    else if (res == 0) {
        return 0;
    }

    return 1;
}

PyDoc_STRVAR(CDBType_has_key__doc__,
"CDB.has_key(key) -> True if CDB has a key 'key', else False");

#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_contains__doc__,
"CDB.__contains__(key) -> True if CDB has a key 'key', else False");
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
"Find the first value of the passed key and return as bytestring\n\
\n\
Note that in case of a unicode key, it will be transformed to utf-8 first.");
#endif
static PyObject *
CDBType_getitem(cdbtype_t *self, PyObject *key)
{
    PyObject *result;
    char *ckey;
    cdb32_len_t csize;
    Py_ssize_t vsize;
    cdb32_ref_t pos, dlen;

    if (!self->cdb32)
        return cdbx_raise_closed();

    if (-1 == cdbx_byte_key(&key, &ckey, &csize))
        return NULL;

    switch (cdb32_find(self->cdb32, ckey, csize)) {
    case -1:
        PyErr_SetFromErrno(PyExc_IOError);
        goto error;

    case 0:
        raise_key_error(key);
        goto error;
    }
    Py_DECREF(key);

    pos = cdb32_datapos(self->cdb32);
    dlen = cdb32_datalen(self->cdb32);
    vsize = (Py_ssize_t)dlen;
    csize = (cdb32_len_t)dlen;
    if ((cdb32_ref_t)vsize != dlen || (cdb32_ref_t)csize != dlen) {
        PyErr_SetString(PyExc_OverflowError, "Value is too long");
        return NULL;
    }

    result = PyBytes_FromStringAndSize(NULL, vsize);
    if (-1 == cdb32_read(self->cdb32, PyBytes_AS_STRING(result), csize, pos)) {
        PyErr_SetFromErrno(PyExc_IOError);
        Py_DECREF(result);
        return NULL;
    }

    return result;

error:
    Py_DECREF(key);
    return NULL;
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
    (lenfunc)CDBType_length,         /* mp_length */
    (binaryfunc)CDBType_getitem,     /* mp_subscript */
    0                                /* mp_ass_subscript */
};

#ifdef METH_COEXIST
PyDoc_STRVAR(CDBType_new__doc__,
"__new__(cls, file)\n\
\n\
Create a CDB instance.\n\
\n\
:Parameters:\n\
  `file` : ``file`` or ``str`` or ``fd``\n\
    Either a (binary) python stream (providing fileno()) or a filename or an\n\
    integer (representing a filedescriptor).\n\
\n\
:Return: New CDB instance\n\
:Rtype: `CDB`");

static PyObject *
CDBType_new(PyTypeObject *cls, PyObject *args, PyObject *kwds);
#endif

static PyMethodDef CDBType_methods[] = {
#ifdef METH_COEXIST
    {"__new__",
     (PyCFunction)CDBType_new,                  METH_COEXIST  |
                                                METH_STATIC   |
                                                METH_KEYWORDS |
                                                METH_VARARGS,
     CDBType_new__doc__},

    {"__iter__",
     (PyCFunction)CDBType_iter,                 METH_NOARGS | METH_COEXIST,
     CDBType_iter__doc__},

    {"__contains__",
     (PyCFunction)CDBType_contains,             METH_O | METH_COEXIST,
     CDBType_contains__doc__},

    {"__getitem__",
     (PyCFunction)CDBType_getitem,              METH_O | METH_COEXIST,
     CDBType_getitem__doc__},
#endif

    {"make",
     (PyCFunction)CDBType_make,                 METH_CLASS    |
                                                METH_KEYWORDS |
                                                METH_VARARGS,
     CDBType_make__doc__},

    {"close",
     (PyCFunction)CDBType_close,                METH_NOARGS,
     CDBType_close__doc__},

    {"fileno",
     (PyCFunction)CDBType_fileno,               METH_NOARGS,
     CDBType_fileno__doc__},

    {"has_key",
    (PyCFunction)CDBType_contains,              METH_O,
     CDBType_has_key__doc__},

    {"keys",
     (PyCFunction)CDBType_keys,                 METH_NOARGS,
     CDBType_keys__doc__},

#if 0
    {"get",
     (PyCFunction)CDBType_get,                  METH_VARARGS,
     CDBType_get__doc__},

    {"getiter",
     (PyCFunction)CDBType_getiter,              METH_VARARGS,
     CDBType_getiter__doc__},

    {"streamget",
     (PyCFunction)CDBType_get,                  METH_VARARGS,
     CDBType_get__doc__},

    {"streamgetiter",
     (PyCFunction)CDBType_getiter,              METH_VARARGS,
     CDBType_getiter__doc__},

    {"items",
     (PyCFunction)CDBType_items,                METH_NOARGS,
     CDBType_items__doc__},

    {"streamitems",
     (PyCFunction)CDBType_streamitems,          METH_NOARGS,
     CDBType_streamitems__doc__},
#endif

    /* Sentinel */
    {NULL, NULL}
};

static PyObject *
CDBType_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"file", NULL};
    PyObject *file_;
    cdbtype_t *self;
    int fd;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
                                     &file_))
        return NULL;

    if (!(self = GENERIC_ALLOC(type)))
        return NULL;

    if (!(self->cdb32 = PyMem_Malloc(sizeof(*self->cdb32))))
        goto error;

    self->cdb32->map = NULL;
    self->pos_cached = 0;
    self->length = -1;
    self->records = -1;

    if (-1 == cdbx_obj_as_fd(file_, "rb", NULL, &self->fp, &self->fp_opened,
                             &fd))
        goto error;

    cdb32_init(self->cdb32, fd);

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
    PyObject *fp;
    cdb32_t *cdb32;

    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    self->pos_cached = 0;
    self->length = -1;
    self->records = -1;

    if ((fp = self->fp)) {
        self->fp = NULL;

        if (self->fp_opened) {
            if (!PyObject_CallMethod(fp, "close", "")) {
                if (PyErr_ExceptionMatches(PyExc_Exception))
                    PyErr_Clear();
            }
        }

        Py_DECREF(fp);
    }

    if ((cdb32 = self->cdb32)) {
        self->cdb32 = NULL;
        cdb32_free(cdb32);
        PyMem_Free(cdb32);
    }

    return 0;
}

DEFINE_GENERIC_DEALLOC(CDBType)

PyDoc_STRVAR(CDBType__doc__,
"CDB(file)");

PyTypeObject CDBType = {
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
