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

#ifndef CDBX_H
#define CDBX_H

#include "cext.h"

/* CDB32 public types (private impl) */
typedef struct cdbx_cdb32_t cdbx_cdb32_t;
typedef struct cdbx_cdb32_iter_t cdbx_cdb32_iter_t;
typedef struct cdbx_cdb32_pointer_t cdbx_cdb32_pointer_t;
typedef struct cdbx_cdb32_maker_t cdbx_cdb32_maker_t;
typedef struct cdbx_cdb32_get_iter_t cdbx_cdb32_get_iter_t;


/*
 * Main CDB type
 */
typedef struct cdbtype_t cdbtype_t;

extern EXT_LOCAL PyTypeObject CDBType;
#define CDBType_Check(op) \
    PyObject_TypeCheck(op, &CDBType)
#define CDBType_CheckExact(op) \
    ((op)->ob_type == &CDBType)

EXT_LOCAL cdbx_cdb32_t *
cdbx_type_get_cdb32(cdbtype_t *);


/*
 * Key iterator
 */
extern EXT_LOCAL PyTypeObject CDBIterType;
EXT_LOCAL PyObject *
cdbx_iter_new(cdbtype_t *, int, int);


/*
 * Maker type
 */
extern EXT_LOCAL PyTypeObject CDBMakerType;
EXT_LOCAL PyObject *
cdbx_maker_new(PyTypeObject *, PyObject *, PyObject *, PyObject *);


/*
 * ************************************************************************
 * CDB 32
 * ************************************************************************
 */

/*
 * Create cdbx_cdb32_t instance
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_create(int, cdbx_cdb32_t **, int);


/*
 * Destroy cdbx_cdb32_t instance
 */
EXT_LOCAL void
cdbx_cdb32_destroy(cdbx_cdb32_t **);


/*
 * Read a pointed value into a bytes object
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_read(cdbx_cdb32_t *, cdbx_cdb32_pointer_t *, PyObject **);


/*
 * Return the FD
 */
EXT_LOCAL int
cdbx_cdb32_fileno(cdbx_cdb32_t *);


/*
 * Check if key is in the CDB
 *
 * Return -1 on error
 * Return 0 on success (no)
 * Return 1 on success (yes)
 *
 */
EXT_LOCAL int
cdbx_cdb32_contains(cdbx_cdb32_t *, PyObject *);


/*
 * Count the number of unique keys (cached)
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_count_keys(cdbx_cdb32_t *, Py_ssize_t *);


#if 0
/*
 * Count the number of records (cached)
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_count_records(cdbx_cdb32_t *, Py_ssize_t *);
# endif


/*
 * Create new get-iterator
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_get_iter_new(cdbx_cdb32_t *, PyObject *, cdbx_cdb32_get_iter_t **);


/*
 * Get next value from get-iterator
 *
 * Return -1 on error
 * Return 0 on success (including exhausted, which emits NULL)
 */
EXT_LOCAL int
cdbx_cdb32_get_iter_next(cdbx_cdb32_get_iter_t *, PyObject **);


/*
 * Destroy get-iterator
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL void
cdbx_cdb32_get_iter_destroy(cdbx_cdb32_get_iter_t **);


/*
 * Create cdbx_cdb32_iter
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_iter_create(cdbx_cdb32_t *, cdbx_cdb32_iter_t **);


/*
 * Destroy cdbx_cdb32_iter_t
 */
EXT_LOCAL void
cdbx_cdb32_iter_destroy(cdbx_cdb32_iter_t **);


/*
 * Find next key/value pair
 *
 * Opaque key and value pointers are returned (invalidated by next call).
 * key will be NULL if the end is reached.
 * value ref may be NULL
 *
 * first == 1 if this is the first occurence of the key, 0 otherwise.
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_iter_next(cdbx_cdb32_iter_t *, cdbx_cdb32_pointer_t **,
                     cdbx_cdb32_pointer_t **, int *);


/*
 * Create new maker instance
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_maker_create(int, cdbx_cdb32_maker_t **);


/*
 * Destroy maker instance
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL void
cdbx_cdb32_maker_destroy(cdbx_cdb32_maker_t **);


/*
 * Return the FD
 */
EXT_LOCAL int
cdbx_cdb32_maker_fileno(cdbx_cdb32_maker_t *);


/*
 * Add a key/value pair
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_maker_add(cdbx_cdb32_maker_t *, PyObject *, PyObject *);


/*
 * Commit the CDB
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_maker_commit(cdbx_cdb32_maker_t *);


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
EXT_LOCAL int
cdbx_unlink(PyObject *);


/*
 * Turn python object into a file descriptor
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_obj_as_fd(PyObject *, char *, PyObject **, PyObject **, int *, int *);


/*
 * set IOError("I/O operation on a closed file") and return NULL
 */
EXT_LOCAL PyObject *
cdbx_raise_closed(void);


/*
 * Convert int object to fd
 *
 * Return -1 on error
 * Return 0 if no error occured.
 */
EXT_LOCAL int
cdbx_fd(PyObject *, int *);


/*
 * Find a particular pyobject attribute
 *
 * Return -1 on error
 * Return 0 if no error occured. attribute will be NULL if it was simply not
 * found.
 */
EXT_LOCAL int
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
EXT_LOCAL PyObject *
cdbx_file_open(PyObject *, const char *);
#else
#define cdbx_file_open(filename, mode) \
    PyObject_CallFunction((PyObject*)&PyFile_Type, "(Os)", filename, mode)
#endif


#endif
