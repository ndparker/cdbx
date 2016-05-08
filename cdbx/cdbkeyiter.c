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
 * Object structure for CDBKeyIterType
 */
typedef struct {
    PyObject_HEAD
    PyObject *weakreflist;

    cdbtype_t *main;  /* CDB instance, we're attached to */
    cdb32_t copy32;  /* cdb struct */
    cdb32_ref_t eod, pos;
} cdbkeyiter_t;


/* ------------------------- BEGIN CDBKeyIterType ------------------------ */

#define CDBKeyIterType_iter PyObject_SelfIter

#define CDB32_READ_NUM(pos) do {                                       \
    if (-1 == cdb32_read(&self->copy32, buf, CDB32_NUM_SIZE, (pos))) { \
        PyErr_SetFromErrno(PyExc_IOError);                             \
        return NULL;                                                   \
    }                                                                  \
    (pos) += CDB32_NUM_SIZE;                                           \
} while (0)

static PyObject *
CDBKeyIterType_iternext(cdbkeyiter_t *self)
{
    char buf[CDB32_NUM_SIZE];
    PyObject *result;
    Py_ssize_t result_size;
    cdb32_ref_t klen, dlen;

    if (!self->main || !cdbx_get_cdb32(self->main))
        return cdbx_raise_closed();

    result = NULL;
    while (self->pos < self->eod) {
        CDB32_READ_NUM(self->pos);
        cdb32_num_unpack(buf, &klen);
        CDB32_READ_NUM(self->pos);
        cdb32_num_unpack(buf, &dlen);
        result_size = (Py_ssize_t) klen;
        if ((cdb32_ref_t)result_size != klen) {
            PyErr_SetString(PyExc_OverflowError, "Key too long");
            return NULL;
        }
        if (!(result = PyBytes_FromStringAndSize(NULL, result_size)))
            return NULL;

        if (-1 == cdb32_read(&self->copy32, PyBytes_AS_STRING(result), klen,
                             self->pos)) {
            PyErr_SetFromErrno(PyExc_IOError);
            goto error;
        }
        self->pos += klen;

        switch (cdb32_find(&self->copy32, PyBytes_AS_STRING(result), klen)) {
        case -1:
            PyErr_SetFromErrno(PyExc_IOError);
            goto error;

        case 0:
            PyErr_SetString(PyExc_ValueError, "CDB Format Error");
            goto error;

        default:
            if (cdb32_datapos(&self->copy32) != self->pos) {
                self->pos += dlen;
                Py_DECREF(result);
                result = NULL;
                continue;
            }
        }

        self->pos += dlen;
        break;
    }

    return result;

error:
    Py_XDECREF(result);
    return NULL;
}

#undef CDB32_READ_NUM

static int
CDBKeyIterType_traverse(cdbkeyiter_t *self, visitproc visit, void *arg)
{
    Py_VISIT((PyObject *)self->main);

    return 0;
}

static int
CDBKeyIterType_clear(cdbkeyiter_t *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    Py_CLEAR(self->main);

    return 0;
}

DEFINE_GENERIC_DEALLOC(CDBKeyIterType)

PyTypeObject CDBKeyIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    EXT_MODULE_PATH ".CDBKeyIterator",                  /* tp_name */
    sizeof(cdbkeyiter_t),                               /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor)CDBKeyIterType_dealloc,                 /* tp_dealloc */
    0,                                                  /* tp_print */
    0,                                                  /* tp_getattr */
    0,                                                  /* tp_setattr */
    0,                                                  /* tp_compare */
    0,                                                  /* tp_repr */
    0,                                                  /* tp_as_number */
    0,                                                  /* tp_as_sequence */
    0,                                                  /* tp_as_mapping */
    0,                                                  /* tp_hash */
    0,                                                  /* tp_call */
    0,                                                  /* tp_str */
    0,                                                  /* tp_getattro */
    0,                                                  /* tp_setattro */
    0,                                                  /* tp_as_buffer */
    Py_TPFLAGS_HAVE_CLASS                               /* tp_flags */
    | Py_TPFLAGS_HAVE_WEAKREFS
    | Py_TPFLAGS_HAVE_ITER
    | Py_TPFLAGS_HAVE_GC,
    0,                                                  /* tp_doc */
    (traverseproc)CDBKeyIterType_traverse,              /* tp_traverse */
    (inquiry)CDBKeyIterType_clear,                      /* tp_clear */
    0,                                                  /* tp_richcompare */
    offsetof(cdbkeyiter_t, weakreflist),                /* tp_weaklistoffset */
    (getiterfunc)CDBKeyIterType_iter,                   /* tp_iter */
    (iternextfunc)CDBKeyIterType_iternext               /* tp_iternext */
};

#define CDB32_READ_NUM(pos) do {                                       \
    if (-1 == cdb32_read(&self->copy32, buf, CDB32_NUM_SIZE, (pos))) { \
        PyErr_SetFromErrno(PyExc_IOError);                             \
        goto error;                                                    \
    }                                                                  \
    pos += CDB32_NUM_SIZE;                                             \
} while (0)

/*
 * Create new key iterator object
 */
PyObject *
cdbx_keyiter_new(cdbtype_t *cdb)
{
    char buf[CDB32_NUM_SIZE];
    cdbkeyiter_t *self;
    cdb32_t *origin;

    if (!(self = GENERIC_ALLOC(&CDBKeyIterType)))
        return NULL;

    self->main = NULL;
    if (!(origin = cdbx_get_cdb32(cdb)))
        return cdbx_raise_closed();

    self->copy32.map = origin->map;
    self->copy32.fd = origin->fd;
    self->copy32.size = origin->size;

    Py_INCREF((PyObject *)cdb);
    self->main = cdb;

    self->pos = 0;
    CDB32_READ_NUM(self->pos);
    cdb32_num_unpack(buf, &self->eod);
    while (self->pos < CDB32_TABLE_SIZE) CDB32_READ_NUM(self->pos);

    return (PyObject *)self;

error:
    Py_DECREF(self);
    return NULL;
}

#undef CDB32_READ_NUM

/* -------------------------- END CDBKeyIterType ------------------------- */
