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

#ifndef CDBX_H
#define CDBX_H

#include "cext.h"
#include "cdb_make.h"
#include "cdb.h"

typedef struct cdb cdb32_t;
typedef struct cdb_make cdb32_make_t;
typedef uint32 cdb32_ref_t;
typedef unsigned int cdb32_len_t;
#define CDB32_NUM_SIZE (4)
#define CDB32_TABLE_SIZE (2048)
#define cdb32_read cdb_read
#define cdb32_find cdb_find
#define cdb32_datapos cdb_datapos
#define cdb32_datalen cdb_datalen
#define cdb32_init cdb_init
#define cdb32_free cdb_free
#define cdb32_num_unpack uint32_unpack
#define cdb32_make_finish cdb_make_finish
#define cdb32_make_add cdb_make_add
#define cdb32_make_start cdb_make_start

/*
 * Main CDB type
 */
typedef struct cdbtype_t cdbtype_t;

extern PyTypeObject CDBType;
#define CDBType_Check(op) \
    PyObject_TypeCheck(op, &CDBType)
#define CDBType_CheckExact(op) \
    ((op)->ob_type == &CDBType)

cdb32_t *
cdbx_get_cdb32(cdbtype_t *);


/*
 * Key iterator
 */
extern PyTypeObject CDBKeyIterType;
PyObject *
cdbx_keyiter_new(cdbtype_t *);


/*
 * Maker type
 */
extern PyTypeObject CDBMakerType;
PyObject *
cdbx_maker_new(PyTypeObject *, PyObject *);


/*
 * If 1, some wrapper objects are added for testing purposes
 */
#ifndef PCDB_TEST
#define PCDB_TEST 0
#endif


#if (PCDB_TEST == 1)
#endif

/*
 * ************************************************************************
 * Generic Utilities
 * ************************************************************************
 */

/*
 * Unlink file
 *
 * Return -1 on error
 * Return 0 on success
 */
int
cdbx_unlink(PyObject *);

/*
 * Turn python object into a file descriptor
 *
 * Return -1 on error
 * Return 0 on success
 */
int
cdbx_obj_as_fd(PyObject *, char *, PyObject **, PyObject **, int *, int *);

/*
 * Transform bytes/unicode key to char/len
 *
 * RC(key_) is increased on success. However, *key_ might be replaced (Py2).
 *
 * Return -1 on error
 * Return 0 on success
 */
int
cdbx_byte_key(PyObject **, char **, cdb32_len_t *);

/*
 * set IOError("I/O operation on a closed file") and return NULL
 */
PyObject *
cdbx_raise_closed(void);

/*
 * Convert int object to fd
 *
 * Return -1 on error
 * Return 0 if no error occured.
 */
int
cdbx_fd(PyObject *, int *);

/*
 * Find a particular pyobject attribute
 *
 * Return -1 on error
 * Return 0 if no error occured. attribute will be NULL if it was simply not
 * found.
 */
int
cdbx_attr(PyObject *, const char *, PyObject **);


/*
 * ************************************************************************
 * Compat
 * ************************************************************************
 */

/*
 * Open a file
 *
 * Return NULL on error
 *
 * Might be a macro.
 */
#ifdef EXT3
PyObject *
cdbx_file_open(PyObject *, const char *);
#else
#define cdbx_file_open(filename, mode) \
    PyObject_CallFunction((PyObject*)&PyFile_Type, "(Os)", filename, mode)
#endif


#endif
