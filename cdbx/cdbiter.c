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

#define FL_ALL   (1 << 0)
#define FL_ITEMS (1 << 1)


/*
 * Object structure for CDBIterType
 */
typedef struct {
    PyObject_HEAD
    PyObject *weakreflist;

    cdbtype_t *main;  /* CDB instance we're attached to */
    cdbx_cdb32_iter_t *iter;  /* Iter state */

    int flags;
} cdbiter_t;


/* -------------------------- BEGIN CDBIterType -------------------------- */

#define CDBIterType_iter PyObject_SelfIter

static PyObject *
CDBIterType_iternext(cdbiter_t *self)
{
    cdbx_cdb32_t *cdb32;
    cdbx_cdb32_pointer_t *key_, *value_;
    PyObject *result, *key, *value;
    int first;

    if (!self->main || !(cdb32 = cdbx_type_get_cdb32(self->main)))
        return cdbx_raise_closed();

    do {
        if (-1 == cdbx_cdb32_iter_next(self->iter, &key_, &value_, &first))
            LCOV_EXCL_LINE_RETURN(NULL);
    } while ((!first && !(self->flags & FL_ALL)) && key_);

    if (!key_)
        return NULL;

    if (-1 == cdbx_cdb32_read(cdb32, key_, &result))
        LCOV_EXCL_LINE_RETURN(NULL);

    if (self->flags & FL_ITEMS) {
        key = result;
        if (-1 == cdbx_cdb32_read(cdb32, value_, &value)) {
            /* LCOV_EXCL_START */

            Py_DECREF(key);
            return NULL;

            /* LCOV_EXCL_STOP */
        }
        if (!(result = PyTuple_New(2))) {
            /* LCOV_EXCL_START */

            Py_DECREF(value);
            Py_DECREF(key);
            return NULL;

            /* LCOV_EXCL_STOP */
        }
        PyTuple_SET_ITEM(result, 0, key);
        PyTuple_SET_ITEM(result, 1, value);
    }

    return result;
}


static int
CDBIterType_traverse(cdbiter_t *self, visitproc visit, void *arg)
{
    Py_VISIT((PyObject *)self->main);

    return 0;
}

static int
CDBIterType_clear(cdbiter_t *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    Py_CLEAR(self->main);

    cdbx_cdb32_iter_destroy(&self->iter);

    return 0;
}

DEFINE_GENERIC_DEALLOC(CDBIterType)

EXT_LOCAL PyTypeObject CDBIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    EXT_MODULE_PATH ".CDBIterator",                     /* tp_name */
    sizeof(cdbiter_t),                                  /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor)CDBIterType_dealloc,                    /* tp_dealloc */
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
    (traverseproc)CDBIterType_traverse,                 /* tp_traverse */
    (inquiry)CDBIterType_clear,                         /* tp_clear */
    0,                                                  /* tp_richcompare */
    offsetof(cdbiter_t, weakreflist),                   /* tp_weaklistoffset */
    (getiterfunc)CDBIterType_iter,                      /* tp_iter */
    (iternextfunc)CDBIterType_iternext                  /* tp_iternext */
};

/*
 * Create new key iterator object
 */
EXT_LOCAL PyObject *
cdbx_iter_new(cdbtype_t *cdb, int items, int all)
{
    cdbiter_t *self;
    cdbx_cdb32_t *cdb32;

    if (!(self = GENERIC_ALLOC(&CDBIterType)))
        LCOV_EXCL_LINE_RETURN(NULL);

    self->main = NULL;
    self->iter = NULL;
    self->flags = 0;

    if (!(cdb32 = cdbx_type_get_cdb32(cdb))) {
        /* LCOV_EXCL_START */

        cdbx_raise_closed();
        goto error;

        /* LCOV_EXCL_STOP */
    }

    if (-1 == cdbx_cdb32_iter_create(cdb32, &self->iter))
        LCOV_EXCL_LINE_GOTO(error);

    Py_INCREF((PyObject *)cdb);
    self->main = cdb;
    if (all)
        self->flags |= FL_ALL;
    if (items)
        self->flags |= FL_ITEMS;

    return (PyObject *)self;

/* LCOV_EXCL_START */
error:
    Py_DECREF(self);
    return NULL;

/* LCOV_EXCL_STOP */
}

/* --------------------------- END CDBIterType --------------------------- */
