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

typedef uint32_t cdb32_off_t;
typedef uint32_t cdb32_len_t;
typedef uint32_t cdb32_hash_t;
typedef unsigned char cdb32_key_t;

/* Pointer */
struct cdbx_cdb32_pointer_t {
    cdb32_off_t offset;
    cdb32_len_t length;
};

/* Slot entry */
typedef struct {
    cdb32_hash_t hash;
    cdb32_off_t offset;
} cdb32_slot_t;

/* Data length */
typedef struct {
    cdb32_len_t klen;
    cdb32_len_t dlen;
} cdb32_dlength_t;

/* Maker state */
#define CDB32_SLOT_LIST_SIZE (1024)
#define CDB32_WRITE_BUF_SIZE (8192)

typedef struct cdb32_slot_list_t {
    struct cdb32_slot_list_t *prev;

    cdb32_slot_t slots[CDB32_SLOT_LIST_SIZE];
} cdb32_slot_list_t;

struct cdbx_cdb32_maker_t {
    unsigned char buf[CDB32_WRITE_BUF_SIZE];
    cdb32_len_t slot_counts[256];

    cdb32_slot_list_t *slot_lists;
    size_t slot_list_index;
    size_t buf_index;
    cdb32_off_t offset;
    cdb32_off_t size;

    int fd;
};

/* Find state */
typedef struct {
    cdbx_cdb32_t *cdb32;
    cdb32_key_t *key;
    cdb32_len_t length;

    cdb32_hash_t hash;
    cdbx_cdb32_pointer_t table;
    cdb32_off_t table_offset;
    cdb32_off_t table_sentinel;
    cdb32_off_t key_disk;
    cdb32_len_t key_num;
} cdb32_find_t;

/* Get state */
struct cdbx_cdb32_get_iter_t {
    cdb32_find_t find;
    PyObject *key;
};

/* Iter state */
struct cdbx_cdb32_iter_t {
    cdbx_cdb32_pointer_t key;
    cdbx_cdb32_pointer_t value;
    cdbx_cdb32_t *cdb32;
    cdb32_off_t pos;
};

/* Main struct */
struct cdbx_cdb32_t {
    PyObject *map;
    Py_ssize_t map_size;
    const void *map_buf;
    const unsigned char *map_pointer;

    cdb32_off_t sentinel;

    Py_ssize_t num_keys;
    Py_ssize_t num_records;

    int fd;
};


#define CDB32_READ_CURPOS ((cdb32_off_t)(0xFFFFFFFF))
#define CDB32_MAX_LEN (0xFFFFFFFF)
#define CDB32_MAX_OFF (0xFFFFFFFF)
#define CDB32_MASK_TABLE (0x7FF)

#define CDB32_HASH_INIT (5381)

#define CDB32_UNPACK(buf) \
    (((buf)[3] << 24) + ((buf)[2] << 16) + ((buf)[1] << 8) + (buf)[0])

#define CDB32_UNPACK_LEN(buf) ((cdb32_len_t)CDB32_UNPACK(buf))
#define CDB32_UNPACK_OFF(buf) ((cdb32_off_t)CDB32_UNPACK(buf))
#define CDB32_UNPACK_HASH(buf) ((cdb32_hash_t)CDB32_UNPACK(buf))

#define CDB32_PACK(num, buf) do {                   \
    (buf)[0] = (unsigned char)((num) & 0xFF);       \
    (buf)[1] = (unsigned char)((num) >> 8) & 0xFF;  \
    (buf)[2] = (unsigned char)((num) >> 16) & 0xFF; \
    (buf)[3] = (unsigned char)((num) >> 24) & 0xFF; \
} while(0)

#define CDB32_PACK_LEN CDB32_PACK
#define CDB32_PACK_OFF CDB32_PACK
#define CDB32_PACK_HASH CDB32_PACK

#define CDB32_SIZEOF_LEN (4)
#define CDB32_SIZEOF_HASH (4)
#define CDB32_SIZEOF_OFF (4)
#define CDB32_SIZEOF_DLENGTH (CDB32_SIZEOF_LEN + CDB32_SIZEOF_LEN)
#define CDB32_SIZEOF_SLOT (CDB32_SIZEOF_HASH + CDB32_SIZEOF_OFF)
#define CDB32_SIZEOF_TPTR (CDB32_SIZEOF_OFF + CDB32_SIZEOF_LEN)
#define CDB32_SIZEOF_TABLE (CDB32_SIZEOF_TPTR << 8)

#define CDB32_OFFSET_SLOT(num) ((num) << 3)

/* hash x (offset(4) + length(4)) % tablesize() */
#define CDB32_PTR_TABLE(hash) (((hash) << 3) & CDB32_MASK_TABLE)

/* table + (((hash // 256) % tablesize(var)) * (hash(4) + offset(4))) */
#define CDB32_PTR_SLOT(hash, table) \
    (table)->offset + CDB32_OFFSET_SLOT((((hash) >> 8) % (table)->length))



/*
 * Transform bytes/unicode key to char/len
 *
 * RC(key_) is increased on success. However, *key_ might be replaced.
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_cstring(PyObject **key_, cdb32_key_t **ckey_, cdb32_len_t *ckeysize_)
{
    Py_ssize_t length;
    PyObject *key = *key_;
    char *cckey;

    Py_INCREF(key);
    if (PyBytes_Check(key)) {
        if (-1 == PyBytes_AsStringAndSize(key, &cckey, &length))
            LCOV_EXCL_LINE_GOTO(error);
        *ckey_ = (cdb32_key_t *)cckey;
    }
    else if (PyUnicode_Check(key)) {
        PyObject *tmp;
        if (!(tmp = PyUnicode_AsLatin1String(key)))
            goto error;

        Py_DECREF(key);
        *key_ = key = tmp;
        if (-1 == PyBytes_AsStringAndSize(key, &cckey, &length))
            LCOV_EXCL_LINE_GOTO(error);
        *ckey_ = (cdb32_key_t *)cckey;
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
    *ckeysize_ = (cdb32_len_t)length;
    if ((Py_ssize_t)*ckeysize_ != length) {
        /* LCOV_EXCL_START */

        PyErr_SetString(PyExc_OverflowError, "Key is too long");
        goto error;

        /* LCOV_EXCL_STOP */
    }

    return 0;

error:
    Py_DECREF(key);
    return -1;
}


/*
 * Read from map into buf
 *
 * If buf is NULL, it works like a simple seek, but with validity check.
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_read_map(cdbx_cdb32_t *self, cdb32_off_t offset, cdb32_len_t len,
               unsigned char *buf)
{
    Py_ssize_t curpos;

    if (offset == CDB32_READ_CURPOS) {
        curpos = self->map_pointer - (unsigned const char *)self->map_buf;

        if (self->map_size - curpos < len) {
            PyErr_SetString(PyExc_IOError, "Format Error");
            return -1;
        }
    }
    else {
        if (offset > self->map_size
            || self->map_size - offset < len) {
            /* LCOV_EXCL_START */

            PyErr_SetString(PyExc_IOError, "Format Error");
            return -1;

            /* LCOV_EXCL_STOP */
        }
        self->map_pointer = (const unsigned char *)self->map_buf + offset;
    }

    if (buf) {
        memcpy(buf, self->map_pointer, len);
        self->map_pointer = (const unsigned char *)self->map_pointer + len;
    }
    return 0;
}


/*
 * Read from file into buf
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_read(cdbx_cdb32_t *self, cdb32_off_t offset, cdb32_len_t len,
           unsigned char *buf)
{
    ssize_t res;

    if (self->map)
        return cdb32_read_map(self, offset, len, buf);

#if CDB32_MAX_LEN > SSIZE_MAX
    while (len > (cdb32_len_t)SSIZE_MAX) {
        if (-1 == cdb32_read(self, offset, (cdb32_len_t)SSIZE_MAX, buf))
            return -1;
        len -= (cdb32_len_t)SSIZE_MAX;
        buf += SSIZE_MAX;
        offset = CDB32_READ_CURPOS;
    }
#endif

    if (len > 0) {
        if (offset != CDB32_READ_CURPOS
            && -1 == lseek(self->fd, (off_t)offset, SEEK_SET)) {
            PyErr_SetFromErrno(PyExc_IOError);
            return -1;
        }

        while (len > 0) {
            switch (res = read(self->fd, buf, len)) {

            /* LCOV_EXCL_START */
            case -1:
                if (errno == EINTR)
                    continue;
                PyErr_SetFromErrno(PyExc_IOError);
                return -1;
            case 0:
                PyErr_SetString(PyExc_IOError, "Format Error");
                return -1;

            /* LCOV_EXCL_STOP */

            default:
                if ((cdb32_len_t)res > len) {
                    /* LCOV_EXCL_START */

                    PyErr_SetString(PyExc_IOError, "Read Error");
                    return -1;

                    /* LCOV_EXCL_STOP */
                }
                len -= (cdb32_len_t)res;
                buf += res;
            }
        }
    }

    return 0;
}


#define CDB32_READ_POINTER(self, offset_, pointer, res) do {                \
    if ((self)->map) {                                                      \
        if (!(res = cdb32_read_map((self), (offset_), CDB32_SIZEOF_TPTR,    \
                                    NULL))) {                               \
            if (((pointer)->length = CDB32_UNPACK_LEN((self)->map_pointer   \
                                                      + CDB32_SIZEOF_OFF))) \
                (pointer)->offset = CDB32_UNPACK_OFF((self)->map_pointer);  \
            (self)->map_pointer += CDB32_SIZEOF_TPTR;                       \
        }                                                                   \
    }                                                                       \
    else {                                                                  \
        unsigned char buf[CDB32_SIZEOF_TPTR];                               \
        if (!(res = cdb32_read((self), (offset_), CDB32_SIZEOF_TPTR,        \
                               buf))) {                                     \
            if (((pointer)->length = CDB32_UNPACK_LEN(buf                   \
                                                      + CDB32_SIZEOF_OFF))) \
                (pointer)->offset = CDB32_UNPACK_OFF(buf);                  \
        }                                                                   \
    }                                                                       \
} while(0)


#define CDB32_READ_SLOT(self, offset_, slot, res) do {                    \
    if ((self)->map) {                                                    \
        if (!(res = cdb32_read_map((self), (offset_), CDB32_SIZEOF_SLOT,  \
                                    NULL))) {                             \
            if (((slot)->offset = CDB32_UNPACK_OFF((self)->map_pointer    \
                                                   + CDB32_SIZEOF_HASH))) \
                (slot)->hash = CDB32_UNPACK_HASH((self)->map_pointer);    \
            (self)->map_pointer += CDB32_SIZEOF_SLOT;                     \
        }                                                                 \
    }                                                                     \
    else {                                                                \
        unsigned char buf[CDB32_SIZEOF_SLOT];                             \
        if (!(res = cdb32_read((self), (offset_), CDB32_SIZEOF_SLOT,      \
                               buf))) {                                   \
            if (((slot)->offset = CDB32_UNPACK_OFF(buf                    \
                                                   + CDB32_SIZEOF_HASH))) \
                (slot)->hash = CDB32_UNPACK_HASH(buf);                    \
        }                                                                 \
    }                                                                     \
} while(0)


#define CDB32_READ_SENTINEL(self, res) do {                               \
    if ((self)->map) {                                                    \
        if (!(res = cdb32_read_map((self), 0, CDB32_SIZEOF_OFF, NULL))) { \
            (self)->sentinel = CDB32_UNPACK_OFF((self)->map_pointer);     \
            (self)->map_pointer += CDB32_SIZEOF_OFF;                      \
        }                                                                 \
    }                                                                     \
    else {                                                                \
        unsigned char buf[CDB32_SIZEOF_OFF];                              \
        if (!(res = cdb32_read((self), 0, CDB32_SIZEOF_OFF, buf)))        \
            (self)->sentinel = CDB32_UNPACK_OFF(buf);                     \
    }                                                                     \
} while(0)


#define CDB32_READ_DLENGTH(self, offset, dlength, res) do {             \
    if ((self)->map) {                                                  \
        if (!(res = cdb32_read_map((self), (offset),                    \
                                   CDB32_SIZEOF_DLENGTH, NULL))) {      \
            (dlength)->klen = CDB32_UNPACK_LEN((self)->map_pointer);    \
            (dlength)->dlen = CDB32_UNPACK_LEN((self)->map_pointer      \
                                              + CDB32_SIZEOF_LEN);      \
            self->map_pointer += CDB32_SIZEOF_DLENGTH;                  \
        }                                                               \
    }                                                                   \
    else {                                                              \
        unsigned char buf[CDB32_SIZEOF_DLENGTH];                        \
        if (!(res = cdb32_read((self), (offset), CDB32_SIZEOF_DLENGTH,  \
                               buf))) {                                 \
            (dlength)->klen = CDB32_UNPACK_LEN(buf);                    \
            (dlength)->dlen = CDB32_UNPACK_LEN(buf + CDB32_SIZEOF_LEN); \
        }                                                               \
    }                                                                   \
} while(0)


/*
 * Byte-compare a key in memory with a key on disk (mapped)
 *
 * Return -1 on error
 * Return 0 on non-match
 * Return 1 on match
 */
static int
cdb32_cmp_key_map(cdbx_cdb32_t *self, cdb32_off_t offset,
                  const cdb32_key_t *key, cdb32_len_t len)
{
    if (len > 0) {
        if (-1 == cdb32_read_map(self, offset, len, NULL))
            LCOV_EXCL_LINE_RETURN(-1);
        if (self->map_pointer == key)
            return 1;
        if (memcmp(self->map_pointer, key, (size_t)len))
            LCOV_EXCL_LINE_RETURN(0);
    }

    return 1;
}


/*
 * Byte-compare a key in memory with a key on disk
 *
 * Return -1 on error
 * Return 0 on non-match
 * Return 1 on match
 */
static int
cdb32_cmp_key_mem(cdbx_cdb32_t *self, cdb32_off_t offset, cdb32_key_t *key,
                  cdb32_len_t len)
{
    unsigned char buf[128];
    cdb32_len_t buflen;

    while (len > 0) {
        if ((buflen = sizeof buf) > len)
            buflen = len;

        if (-1 == cdb32_read(self, offset, buflen, buf))
            LCOV_EXCL_LINE_RETURN(-1);
        if (memcmp(buf, key, (size_t)buflen))
            LCOV_EXCL_LINE_RETURN(0);
        offset += buflen;
        key += buflen;
        len -= buflen;
    }

    return 1;
}


/*
 * Byte-compare a key on disk with a key on disk
 *
 * Return -1 on error
 * Return 0 on non-match
 * Return 1 on match
 */
static int
cdb32_cmp_key_disk(cdbx_cdb32_t *self, cdb32_off_t offset, cdb32_off_t key,
                   cdb32_len_t len)
{
    unsigned char sbuf[64];
    unsigned char dbuf[sizeof sbuf];
    cdb32_len_t buflen;

    if (offset == key)
        return 1;

    while (len > 0) {
        if ((buflen = sizeof sbuf) > len)
            buflen = len;

        if (-1 == cdb32_read(self, key, buflen, sbuf))
            LCOV_EXCL_LINE_RETURN(-1);
        if (-1 == cdb32_read(self, offset, buflen, dbuf))
            LCOV_EXCL_LINE_RETURN(-1);
        if (memcmp(sbuf, dbuf, (size_t)buflen))
            LCOV_EXCL_LINE_RETURN(0);
        offset += buflen;
        key += buflen;
        len -= buflen;
    }

    return 1;
}


/*
 * Calculate Hash of a key (on disk)
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_hash_disk(cdbx_cdb32_t *self, cdb32_off_t offset, cdb32_len_t len,
                cdb32_hash_t *hash)
{
    unsigned char buf[256];
    unsigned char *key;
    cdb32_len_t buflen;
    cdb32_hash_t result = CDB32_HASH_INIT;

    if (len > 0 && offset != CDB32_READ_CURPOS) {
        if (-1 == lseek(self->fd, (off_t)offset, SEEK_SET)) {
            /* LCOV_EXCL_START */

            PyErr_SetFromErrno(PyExc_IOError);
            return -1;

            /* LCOV_EXCL_STOP */
        }
    }

    while (len > 0) {
        if ((buflen = sizeof buf) > len)
            buflen = len;

        if (-1 == cdb32_read(self, CDB32_READ_CURPOS, buflen, buf))
            LCOV_EXCL_LINE_RETURN(-1);
        len -= buflen;
        key = buf;
        while (buflen--)
            result = (result + (result << 5)) ^ (*key++);
    }
    *hash = result;

    return 0;
}


/*
 * Calculate Hash of a key (in memory)
 *
 * Return: Hash
 */
static cdb32_hash_t
cdb32_hash_mem(const cdb32_key_t *key, cdb32_len_t len)
{
    cdb32_hash_t result = CDB32_HASH_INIT;

    while (len--)
        result = (result + (result << 5)) ^ (*key++);

    return result;
}


/*
 * Find a key/value pair
 *
 * Return -1 on error
 * Return 0 on success (includes not found [value.offset = 0])
 */
static int
cdb32_find(cdb32_find_t *self, cdbx_cdb32_pointer_t *value)
{
    cdb32_slot_t slot = {0};
    cdb32_dlength_t dlength = {0};
    int res;

    /* If this is the first key, initialize the rest of the structure */
    if (!self->key_num) {
        if (self->key_disk) {
            if (self->cdb32->map) {
                if (-1 == cdb32_read_map(self->cdb32, self->key_disk,
                                         self->length, NULL))
                    LCOV_EXCL_LINE_RETURN(-1);

                self->hash = cdb32_hash_mem(
                    (const cdb32_key_t *)self->cdb32->map_pointer, self->length
                );
            }
            else if (-1 == cdb32_hash_disk(self->cdb32, self->key_disk,
                                           self->length, &self->hash))
                LCOV_EXCL_LINE_RETURN(-1);
        }
        else {
            self->hash = cdb32_hash_mem(self->key, self->length);
        }
        CDB32_READ_POINTER(self->cdb32, CDB32_PTR_TABLE(self->hash),
                           &self->table, res);
        if (-1 == res)
            LCOV_EXCL_LINE_RETURN(-1);
        if (!self->table.length) {
            value->offset = 0;
            return 0;
        }

        self->table_offset = CDB32_PTR_SLOT(self->hash, &self->table);
        self->table_sentinel = self->table.offset
                               + CDB32_OFFSET_SLOT(self->table.length);
    }

    /* Now look it up */
    while (self->key_num < self->table.length) {
        CDB32_READ_SLOT(self->cdb32, self->table_offset, &slot, res);
        if (-1 == res)
            LCOV_EXCL_LINE_RETURN(-1);

        if (!slot.offset) {
            value->offset = 0;
            return 0;
        }
        ++self->key_num;
        if ((self->table_offset += CDB32_SIZEOF_SLOT) == self->table_sentinel)
            self->table_offset = self->table.offset;

        if (slot.hash == self->hash) {
            CDB32_READ_DLENGTH(self->cdb32, slot.offset, &dlength, res);
            if (-1 == res)
                LCOV_EXCL_LINE_RETURN(-1);
            if (dlength.klen == self->length) {
                if (self->key_disk) {
                    if (self->cdb32->map) {
                        if (!(res = cdb32_read_map(self->cdb32,
                                                   self->key_disk,
                                                   self->length, NULL)))
                            res = cdb32_cmp_key_map(self->cdb32,
                                                    slot.offset
                                                        + CDB32_SIZEOF_DLENGTH,
                                                    self->cdb32->map_pointer,
                                                    self->length);
                    }
                    else {
                        res = cdb32_cmp_key_disk(self->cdb32,
                                                 slot.offset
                                                    + CDB32_SIZEOF_DLENGTH,
                                                 self->key_disk, self->length);
                    }
                }
                else if (self->cdb32->map) {
                    res = cdb32_cmp_key_map(self->cdb32,
                                            slot.offset + CDB32_SIZEOF_DLENGTH,
                                            self->key, self->length);
                }
                else {
                    res = cdb32_cmp_key_mem(self->cdb32,
                                            slot.offset + CDB32_SIZEOF_DLENGTH,
                                            self->key, self->length);
                }
                switch (res) {
                /* LCOV_EXCL_START */
                case -1:
                    return -1;
                /* LCOV_EXCL_STOP */

                case 1:
                    value->offset = slot.offset + CDB32_SIZEOF_DLENGTH
                                    + self->length;
                    value->length = dlength.dlen;
                    return 1;
                }
            }
        }
    }

    value->offset = 0;
    return 0;
}


/*
 * Count and cache keys and records
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_count_records(cdbx_cdb32_t *self)
{
    cdb32_off_t pos, sentinel;
    cdbx_cdb32_pointer_t pointer;
    cdb32_dlength_t dlength = {0};
    int res;
    Py_ssize_t keys, records;
    cdb32_find_t find;

    if (!(sentinel = self->sentinel)) {
        CDB32_READ_SENTINEL(self, res);
        if (-1 == res)
            LCOV_EXCL_LINE_RETURN(-1);
        sentinel = self->sentinel;
    }

    pos = CDB32_SIZEOF_TABLE;
    keys = 0;
    records = 0;

    while (pos < sentinel) {
        if (!(records < PY_SSIZE_T_MAX)) {
            /* LCOV_EXCL_START */

            PyErr_SetString(PyExc_OverflowError, "Number of keys too big");
            return -1;

            /* LCOV_EXCL_STOP */
        }

        /* Find key + data length */
        CDB32_READ_DLENGTH(self, pos, &dlength, res);
        if (-1 == res)
            LCOV_EXCL_LINE_RETURN(-1);
        pos += CDB32_SIZEOF_DLENGTH;

        find.cdb32 = self;
        find.key_num = 0;
        find.length = dlength.klen;
        find.key_disk = pos;
        pos += dlength.klen;
        if (-1 == cdb32_find(&find, &pointer))
            LCOV_EXCL_LINE_RETURN(-1);

        if (!pointer.offset) {
            /* LCOV_EXCL_START */

            PyErr_SetString(PyExc_IOError, "Format Error");
            return -1;

            /* LCOV_EXCL_STOP */
        }
        ++records;

        if (pointer.offset == pos)
            ++keys;
        pos += dlength.dlen;
    }

    self->num_records = records;
    self->num_keys = keys;
    return 0;
}


/*
 * mmap the cdb file
 *
 * If it doesn't work, ignore. In this case we'll just seek & read later.
 *
 * Return -1 on error
 * Return 0 on success
 */

#ifdef EXT3
#define PyInt_FromSsize_t PyLong_FromSsize_t
#define PyInt_FromLong PyLong_FromLong
#endif

static int
cdb32_mmap(cdbx_cdb32_t *self)
{
    unsigned char *buf, *cp, *sp;
    PyObject *module, *func, *args, *kwargs, *tmp;
    cdb32_len_t len;
    cdb32_off_t size = CDB32_SIZEOF_TABLE;
    size_t size_;
    int res;

    if (!(module = PyImport_ImportModule("mmap")))
        LCOV_EXCL_LINE_RETURN(-1);

    if (!(buf = PyMem_Malloc(CDB32_SIZEOF_TABLE)))
        LCOV_EXCL_LINE_GOTO(error_module);

    if (-1 == cdb32_read(self, 0, CDB32_SIZEOF_TABLE, buf))
        LCOV_EXCL_LINE_GOTO(error_buf);

    /* Find the last non-empty table entry, use that to find the end of the
     * file. Each table entry is a tuple of (offset, length), 4 bytes each.
     */
    cp = buf + CDB32_SIZEOF_TABLE - CDB32_SIZEOF_LEN;
    sp = buf + CDB32_SIZEOF_OFF;
    while (!(len = CDB32_UNPACK_LEN(cp))) {
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
        if (!(cp > sp))
            break;
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif
        cp -= CDB32_SIZEOF_OFF + CDB32_SIZEOF_LEN;
    }

    /* Seek to the end */
    if (len) {
        cp -= CDB32_SIZEOF_OFF;
        size = CDB32_UNPACK_OFF(cp);
        len *= CDB32_SIZEOF_SLOT;
        if ((CDB32_MAX_OFF == len) || (CDB32_MAX_OFF - len) < size - 1) {
            /* LCOV_EXCL_START */

            PyErr_SetNone(PyExc_OverflowError);
            goto error_buf;

            /* LCOV_EXCL_STOP */
        }
        size += len;

        if (-1 == lseek(self->fd, size - 1, SEEK_SET)
            || -1 == lseek(self->fd, 0, SEEK_SET)) {
            PyErr_SetFromErrno(PyExc_IOError);
            goto error_buf;
        }

        size_ = size;
        if (size_ > PY_SSIZE_T_MAX) {
            /* LCOV_EXCL_START */

            PyErr_SetNone(PyExc_OverflowError);
            goto error_buf;

            /* LCOV_EXCL_STOP */
        }
    }

    if (-1 == cdbx_attr(module, "mmap", &func) || !func)
        goto error_buf;
    if (!(kwargs = PyDict_New()))
        LCOV_EXCL_LINE_GOTO(error_func);

    if (-1 == cdbx_attr(module, "ACCESS_READ", &tmp) || !tmp)
        LCOV_EXCL_LINE_GOTO(error_kwargs);
    res = PyDict_SetItemString(kwargs, "access", tmp);
    Py_DECREF(tmp);
    if (-1 == res)
        LCOV_EXCL_LINE_GOTO(error_kwargs);

    if (!(tmp = PyInt_FromLong(self->fd)))
        LCOV_EXCL_LINE_GOTO(error_kwargs);
    res = PyDict_SetItemString(kwargs, "fileno", tmp);
    Py_DECREF(tmp);
    if (-1 == res)
        LCOV_EXCL_LINE_GOTO(error_kwargs);

    if (!(tmp = PyInt_FromSsize_t((Py_ssize_t)size)))
        LCOV_EXCL_LINE_GOTO(error_kwargs);
    res = PyDict_SetItemString(kwargs, "length", tmp);
    Py_DECREF(tmp);
    if (-1 == res)
        LCOV_EXCL_LINE_GOTO(error_kwargs);

    if (!(args = PyTuple_New(0)))
        LCOV_EXCL_LINE_GOTO(error_kwargs);

    tmp = PyObject_Call(func, args, kwargs);
    Py_DECREF(args);
    Py_DECREF(kwargs);
    Py_DECREF(func);
    PyMem_Free(buf);
    Py_DECREF(module);
    if (!tmp)
        LCOV_EXCL_LINE_RETURN(-1);

#ifdef EXT2
    if (-1 == PyObject_AsReadBuffer(tmp, &self->map_buf, &self->map_size)) {
        /* LCOV_EXCL_START */

        Py_DECREF(tmp);
        return -1;

        /* LCOV_EXCL_STOP */
    }
    self->map = tmp;
#else
    {
        Py_buffer view;

        if (-1 == PyObject_GetBuffer(tmp, &view, PyBUF_SIMPLE)) {
            /* LCOV_EXCL_START */

            Py_DECREF(tmp);
            return -1;

            /* LCOV_EXCL_STOP */
        }
        self->map_buf = view.buf;
        self->map_size = view.len;
        self->map = tmp;
    }
#endif
    self->map_pointer = self->map_buf;

    return 0;

/* LCOV_EXCL_START */
error_kwargs:
    Py_DECREF(kwargs);
error_func:
    Py_DECREF(func);
/* LCOV_EXCL_STOP */

error_buf:
    PyMem_Free(buf);
error_module:
    Py_DECREF(module);

    return -1;
}

#ifdef EXT3
#undef PyInt_FromLong
#undef PyInt_FromSsize_t
#endif


/*
 * Write a buffer on disk
 */
static int
cdb32_maker_write(int fd, unsigned char *buf, size_t len)
{
    ssize_t res;

    while (len > (size_t)SSIZE_MAX) {
        if (-1 == cdb32_maker_write(fd, buf, (size_t)SSIZE_MAX))
            LCOV_EXCL_LINE_RETURN(-1);
        len -= (size_t)SSIZE_MAX;
        buf += SSIZE_MAX;
    }

    while (len > 0) {
        switch (res = write(fd, buf, len)) {

        /* LCOV_EXCL_START */
        case -1:
            if (errno == EINTR)
                continue;
            PyErr_SetFromErrno(PyExc_IOError);
            return -1;
        /* LCOV_EXCL_STOP */

        default:
            if ((size_t)res > len) {
                /* LCOV_EXCL_START */

                PyErr_SetString(PyExc_IOError, "Write Error");
                return -1;

                /* LCOV_EXCL_STOP */
            }
            len -= (size_t)res;
            buf += res;
        }
    }

    return 0;
}

/*
 * Flush make writer buffer
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_maker_buf_flush(cdbx_cdb32_maker_t *self)
{
    size_t len;

    len = self->buf_index;
    self->buf_index = 0;
    return cdb32_maker_write(self->fd, self->buf, len);
}


/*
 * Write string and optionally hash it on the go
 *
 * Return -1 on error
 * Return 0 on success
 */
static int
cdb32_maker_buf_write(cdbx_cdb32_maker_t *self, cdb32_key_t *key,
                      cdb32_len_t len, cdb32_hash_t *hash)
{
    cdb32_hash_t result = CDB32_HASH_INIT;
    size_t buflen;

    if (CDB32_MAX_OFF == len
        || (CDB32_MAX_OFF - len) < self->size - 1) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_OverflowError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    self->size += len;
    self->offset += len;
    while (len > 0) {
        if ((buflen = (size_t)len) > (CDB32_WRITE_BUF_SIZE - self->buf_index))
            buflen = CDB32_WRITE_BUF_SIZE - self->buf_index;

        len -= (cdb32_len_t)buflen;
        while (buflen-- > 0) {
            if (hash)
                result = (result + (result << 5)) ^ (*key);
            self->buf[self->buf_index++] = *key++;
        }
        if (self->buf_index == CDB32_WRITE_BUF_SIZE
            && -1 == cdb32_maker_buf_flush(self))
            LCOV_EXCL_LINE_RETURN(-1);
    }

    if (hash)
        *hash = result;

    return 0;
}


/*
 * Create cdbx_cdb32_t instance
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_create(int fd, cdbx_cdb32_t **cdb32_, int mmap)
{
    cdbx_cdb32_t *self;

    if (!(self = PyMem_Malloc(sizeof *self))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    self->map = NULL;
    self->fd = fd;
    self->num_keys = -1;
    self->num_records = -1;
    self->sentinel = 0;
    if (mmap) {
        if (-1 == cdb32_mmap(self)) {
            if (mmap == -1) {
                PyErr_Clear();
            }
            else {
                PyMem_Free(self);
                return -1;
            }
        }
    }

    *cdb32_ = self;

    return 0;
}

/*
 * Destroy cdbx_cdb32_t instance
 */
EXT_LOCAL void
cdbx_cdb32_destroy(cdbx_cdb32_t **cdb32_)
{
    cdbx_cdb32_t *self;

    if (cdb32_ && (self = *cdb32_)) {
        *cdb32_ = NULL;

        Py_CLEAR(self->map);
        PyMem_Free(self);
    }
}


/*
 * Return the FD
 */
EXT_LOCAL int
cdbx_cdb32_fileno(cdbx_cdb32_t *self)
{
    return self->fd;
}


/*
 * Check if key is in the CDB
 *
 * Return -1 on error
 * Return 0 on success (no)
 * Return 1 on success (yes)
 *
 */
EXT_LOCAL int
cdbx_cdb32_contains(cdbx_cdb32_t *self, PyObject *key)
{
    cdb32_find_t find = {0};
    cdbx_cdb32_pointer_t value;

    if (-1 == cdb32_cstring(&key, &find.key, &find.length))
        return -1;

    find.cdb32 = self;
    if (-1 == cdb32_find(&find, &value))
        LCOV_EXCL_LINE_GOTO(error);

    Py_DECREF(key);
    return !!value.offset;

/* LCOV_EXCL_START */
error:
    Py_DECREF(key);
    return -1;
/* LCOV_EXCL_STOP */
}


/*
 * Count the number of unique keys (cached)
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_count_keys(cdbx_cdb32_t *self, Py_ssize_t *result)
{
    if (self->num_keys == -1) {
        if (-1 == cdb32_count_records(self))
            LCOV_EXCL_LINE_RETURN(-1);
    }

    *result = self->num_keys;
    return 0;
}


#if 0
/*
 * Count the number of records (cached)
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_count_records(cdbx_cdb32_t *self, Py_ssize_t *result)
{
    if (self->num_records == -1) {
        if (-1 == cdb32_count_records(self))
            return -1;
    }

    *result = self->num_records;
    return 0;
}
#endif


/*
 * Create cdbx_cdb32_iter_t
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_iter_create(cdbx_cdb32_t *cdb32, cdbx_cdb32_iter_t **result)
{
    cdbx_cdb32_iter_t *self;
    int res;

    if (!(self = PyMem_Malloc(sizeof *self))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    if (!cdb32->sentinel) {
        CDB32_READ_SENTINEL(cdb32, res);
        if (-1 == res)
            LCOV_EXCL_LINE_GOTO(error);
    }

    self->cdb32 = cdb32;
    self->pos = CDB32_SIZEOF_TABLE;
    *result = self;

    return 0;

/* LCOV_EXCL_START */
error:
    PyMem_Free(self);
    return -1;
/* LCOV_EXCL_STOP */
}


/*
 * Destroy cdbx_cdb32_iter_t
 */
EXT_LOCAL void
cdbx_cdb32_iter_destroy(cdbx_cdb32_iter_t **self_)
{
    cdbx_cdb32_iter_t *self;

    if (self_ && (self = *self_)) {
        *self_ = NULL;
        PyMem_Free(self);
    }
}


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
cdbx_cdb32_iter_next(cdbx_cdb32_iter_t *self,
                     cdbx_cdb32_pointer_t **key_,
                     cdbx_cdb32_pointer_t **value_,
                     int *first_)
{
    cdb32_find_t find;
    cdb32_dlength_t dlength = {0};
    int res;

    if (self->pos < self->cdb32->sentinel) {
        /* Find key + data length */
        CDB32_READ_DLENGTH(self->cdb32, self->pos, &dlength, res);
        if (-1 == res)
            LCOV_EXCL_LINE_RETURN(-1);
        self->pos += CDB32_SIZEOF_DLENGTH;

        find.cdb32 = self->cdb32;
        find.key_num = 0;
        find.length = dlength.klen;
        find.key_disk = self->pos;
        self->key.offset = self->pos;
        self->key.length = dlength.klen;
        self->pos += dlength.klen;
        if (-1 == cdb32_find(&find, &self->value))
            LCOV_EXCL_LINE_RETURN(-1);

        if (!self->value.offset) {
            /* LCOV_EXCL_START */

            PyErr_SetString(PyExc_IOError, "Format Error");
            return -1;

            /* LCOV_EXCL_STOP */
        }
        *first_ = (self->value.offset == self->pos);
        *key_ = &self->key;
        if (value_) {
            self->value.offset = self->pos;
            self->value.length = dlength.dlen;
            *value_ = &self->value;
        }
        self->pos += dlength.dlen;

        return 0;
    }

    *first_ = 1;
    *key_ = NULL;
    return 0;
}


/*
 * Read a pointed value into a bytes object
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_read(cdbx_cdb32_t *self, cdbx_cdb32_pointer_t *value,
                PyObject **result_)
{
    PyObject *result;
    Py_ssize_t length;

    length = (Py_ssize_t)value->length;
    if (length < 0 || (cdb32_off_t)length != value->length) {
        /* LCOV_EXCL_START */

        PyErr_SetString(PyExc_OverflowError, "Value too long");
        return -1;

        /* LCOV_EXCL_STOP */
    }

    if (!(result = PyBytes_FromStringAndSize(NULL, length)))
        LCOV_EXCL_LINE_RETURN(-1);

    if (-1 == cdb32_read(self, value->offset,
                         (cdb32_len_t)PyBytes_GET_SIZE(result),
                         (unsigned char *)PyBytes_AS_STRING(result)))
        LCOV_EXCL_LINE_GOTO(error);

    *result_ = result;
    return 0;

/* LCOV_EXCL_START */
error:
    Py_DECREF(result);
    return -1;
/* LCOV_EXCL_STOP */
}


/*
 * Create new maker instance
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_maker_create(int fd, cdbx_cdb32_maker_t **self_)
{
    cdbx_cdb32_maker_t *self;
    int j;

    if (-1 == lseek(fd, 0, SEEK_SET)
        || -1 == ftruncate(fd, 0)
        || -1 == lseek(fd, CDB32_SIZEOF_TABLE, SEEK_SET)) {
        /* LCOV_EXCL_START */

        PyErr_SetFromErrno(PyExc_IOError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    if (!(self = PyMem_Malloc(sizeof *self))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    for (j = 0; j < 256; ++j)
        self->slot_counts[j] = 0;
    self->slot_lists = NULL;
    self->slot_list_index = 0;
    self->buf_index = 0;
    self->size = self->offset = CDB32_SIZEOF_TABLE;
    self->fd = fd;

    *self_ = self;
    return 0;
}


/*
 * Destroy maker instance
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL void
cdbx_cdb32_maker_destroy(cdbx_cdb32_maker_t **self_)
{
    cdbx_cdb32_maker_t *self;
    cdb32_slot_list_t *list;

    if (self_ && (self = *self_)) {
        *self_ = NULL;

        while ((list = self->slot_lists)) {
            self->slot_lists = list->prev;
            PyMem_Free(list);
        }

        PyMem_Free(self);
    }
}


/*
 * Return the FD
 */
EXT_LOCAL int
cdbx_cdb32_maker_fileno(cdbx_cdb32_maker_t *self)
{
    return self->fd;
}


/*
 * Add a key/value pair
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_maker_add(cdbx_cdb32_maker_t *self, PyObject *key, PyObject *value)
{
    cdb32_key_t *ckey, *cvalue;
    cdb32_slot_list_t *slot_list;
    cdb32_len_t lkey, lvalue;
    cdb32_off_t offset;
    cdb32_hash_t hash;
    unsigned char *buf;

    if ((CDB32_MAX_OFF - CDB32_SIZEOF_DLENGTH) < self->size - 1) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_OverflowError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    if (-1 == cdb32_cstring(&key, &ckey, &lkey))
        return -1;
    if (-1 == cdb32_cstring(&value, &cvalue, &lvalue))
        LCOV_EXCL_LINE_GOTO(error_key);

    if (((CDB32_WRITE_BUF_SIZE - self->buf_index) <
            (CDB32_SIZEOF_DLENGTH)) && (-1 == cdb32_maker_buf_flush(self)))
        LCOV_EXCL_LINE_GOTO(error_value);

    buf = self->buf + self->buf_index;
    CDB32_PACK_LEN(lkey, buf);
    buf += CDB32_SIZEOF_LEN;
    CDB32_PACK_LEN(lvalue, buf);
    self->buf_index += CDB32_SIZEOF_DLENGTH;
    offset = self->offset;
    self->size += CDB32_SIZEOF_DLENGTH;
    self->offset += CDB32_SIZEOF_DLENGTH;

    if (-1 == cdb32_maker_buf_write(self, ckey, lkey, &hash))
        goto error_value;
    if (-1 == cdb32_maker_buf_write(self, cvalue, lvalue, NULL))
        goto error_value;

    /* Slots will be doubled -> times 2 */
    if ((CDB32_MAX_OFF - (CDB32_SIZEOF_SLOT + CDB32_SIZEOF_SLOT)) <
            self->size - 1) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_OverflowError);
        goto error_value;

        /* LCOV_EXCL_STOP */
    }
    self->size += CDB32_SIZEOF_SLOT + CDB32_SIZEOF_SLOT;

    if (!(slot_list = self->slot_lists)
        || !(self->slot_list_index < CDB32_SLOT_LIST_SIZE)) {
        if (!(slot_list = PyMem_Malloc(sizeof *slot_list))) {
            /* LCOV_EXCL_START */

            PyErr_SetNone(PyExc_MemoryError);
            goto error_value;

            /* LCOV_EXCL_STOP */
        }
        self->slot_list_index = 0;
        slot_list->prev = self->slot_lists;
        self->slot_lists = slot_list;
    }
    slot_list->slots[self->slot_list_index].hash = hash;
    slot_list->slots[self->slot_list_index++].offset = offset;
    ++self->slot_counts[hash & 0xFF];

    Py_DECREF(value);
    Py_DECREF(key);
    return 0;

/* LCOV_EXCL_START */
error_value:
    Py_DECREF(value);
error_key:
    Py_DECREF(key);
    return -1;
/* LCOV_EXCL_STOP */
}


/*
 * Commit the CDB
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_maker_commit(cdbx_cdb32_maker_t *self)
{
    unsigned char *table, *tp, *buf;
    cdb32_slot_t *sorted, *slots, *sp;
    cdb32_off_t *starts;
    cdb32_slot_list_t *slot_list;
    cdb32_off_t offset, slot;
    cdb32_len_t count, max_slots, num_slot;
    size_t index;
    int j;

    /*
     * Count the number of filled slots per bucket.
     *
     * `starts` contains the offset for each bucket in `sorted` later.
     */
    if (!(starts = PyMem_Malloc(256 * sizeof *starts))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        return -1;

        /* LCOV_EXCL_STOP */
    }
    for (count=0, max_slots=0, j=0; j < 256; ++j) {
        count += self->slot_counts[j];
        starts[j] = count;  /* We will fill them backwards below */
        if (self->slot_counts[j] > max_slots)
            max_slots = self->slot_counts[j];
    }

    if (!(sorted = PyMem_Malloc(count * sizeof *sorted))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        goto error_starts;

        /* LCOV_EXCL_STOP */
    }

    /*
     * This loop puts all hash slots into their buckets
     * and they are ordered by insertion order (per bucket)
     */
    slot_list = self->slot_lists;
    index = self->slot_list_index;
    while (slot_list) {
        while (index--) {
            sorted[--starts[slot_list->slots[index].hash & 0xFF]] =
                slot_list->slots[index];
        }

        index = CDB32_SLOT_LIST_SIZE;
        slot_list = slot_list->prev;
    }

    /*
     * The slots are the buffer for the actual later slot table,
     * which is later serialized on disk. It contains twice the number of
     * items as there are slots, so obviously there will be free slots,
     * which act as end-of-search markers
     */
    if (!(slots = PyMem_Malloc(max_slots * 2 * sizeof *slots))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        goto error_sorted;

        /* LCOV_EXCL_STOP */
    }

    /*
     * `table` contains the pointers to the slot tables. This will be the
     * very beginning of the file.
     */
    if (!(tp = table = PyMem_Malloc(CDB32_SIZEOF_TABLE))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        goto error_slots;

        /* LCOV_EXCL_STOP */
    }

    /*
     * Now fill ALL the slot tables and write them to disk
     */
    sp = sorted;
    offset = self->offset;
    for (j = 0; j < 256; ++j) {
        count = self->slot_counts[j];
        max_slots = count + count;

        /* Enter offset and slot table length into bucket table */
        CDB32_PACK_OFF(offset, tp);
        tp += CDB32_SIZEOF_OFF;
        CDB32_PACK_LEN(max_slots, tp);
        tp += CDB32_SIZEOF_LEN;

        /* Reset all slots */
        for (num_slot = 0; num_slot < max_slots; ++num_slot) {
            slots[num_slot].offset = 0;
            slots[num_slot].hash = 0;
        }

        /* Generate the real slot data */
        for (num_slot = 0; num_slot < count; ++num_slot) {
            /* Find search slot and skip already filled slots */
            slot = (sp->hash >> 8) % max_slots;
            while (slots[slot].offset)
                slot = (slot + 1) % max_slots;
            slots[slot] = *sp++;
        }

        /* Write it on disk */
        buf = self->buf;
        for (num_slot = 0; num_slot < max_slots; ++num_slot) {

            /* LCOV_EXCL_START */
            if (((CDB32_WRITE_BUF_SIZE - self->buf_index)
                  < (CDB32_SIZEOF_SLOT))
                && (-1 == cdb32_maker_buf_flush(self)))
                goto error_table;
            /* LCOV_EXCL_STOP */

            buf = self->buf + self->buf_index;
            CDB32_PACK_HASH(slots[num_slot].hash, buf);
            buf += CDB32_SIZEOF_HASH;
            CDB32_PACK_OFF(slots[num_slot].offset, buf);
            self->buf_index += CDB32_SIZEOF_SLOT;
            offset += CDB32_SIZEOF_SLOT;
        }
    }

    if (-1 == cdb32_maker_buf_flush(self))
        LCOV_EXCL_LINE_GOTO(error_table);

    if (-1 == lseek(self->fd, 0, SEEK_SET)) {
        PyErr_SetFromErrno(PyExc_IOError);
        goto error_table;
    }
    if (-1 == cdb32_maker_write(self->fd, table, CDB32_SIZEOF_TABLE))
        LCOV_EXCL_LINE_GOTO(error_table);

    PyMem_Free(table);
    PyMem_Free(slots);
    PyMem_Free(sorted);
    PyMem_Free(starts);
    return 0;

error_table:
    PyMem_Free(table);
error_slots:
    PyMem_Free(slots);
error_sorted:
    PyMem_Free(sorted);
error_starts:
    PyMem_Free(starts);
    return -1;
}


/*
 * Create new get-iterator
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL int
cdbx_cdb32_get_iter_new(cdbx_cdb32_t *cdb32, PyObject *key,
                        cdbx_cdb32_get_iter_t **result_)
{
    cdbx_cdb32_get_iter_t *result;

    if (!(result = PyMem_Malloc(sizeof *result))) {
        /* LCOV_EXCL_START */

        PyErr_SetNone(PyExc_MemoryError);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    if (-1 == cdb32_cstring(&key, &result->find.key, &result->find.length)) {
        PyMem_Free(result);
        return -1;
    }

    result->find.cdb32 = cdb32;
    result->find.key_num = 0;
    result->find.key_disk = 0;
    result->key = key;
    *result_ = result;
    return 0;
}


/*
 * Destroy get-iterator
 *
 * Return -1 on error
 * Return 0 on success
 */
EXT_LOCAL void
cdbx_cdb32_get_iter_destroy(cdbx_cdb32_get_iter_t **self_)
{
    cdbx_cdb32_get_iter_t *self;

    if (self_ && (self = *self_)) {
        *self_ = NULL;

        Py_CLEAR(self->key);
        PyMem_Free(self);
    }
}


/*
 * Get next value from get-iterator
 *
 * Return -1 on error
 * Return 0 on success (including exhausted, which emits NULL)
 */
EXT_LOCAL int
cdbx_cdb32_get_iter_next(cdbx_cdb32_get_iter_t *self, PyObject **value_)
{
    cdbx_cdb32_pointer_t value;
    PyObject *result;
    Py_ssize_t vsize;

    if (-1 == cdb32_find(&self->find, &value))
        LCOV_EXCL_LINE_RETURN(-1);
    if (!value.offset) {
        *value_ = NULL;
        return 0;
    }

    vsize = (Py_ssize_t)value.length;
    if (vsize < 0 || (cdb32_len_t)vsize != value.length) {
        /* LCOV_EXCL_START */

        PyErr_SetString(PyExc_OverflowError, "Value is too long");
        return -1;

        /* LCOV_EXCL_STOP */
    }

    if (!(result = PyBytes_FromStringAndSize(NULL, vsize)))
        LCOV_EXCL_LINE_RETURN(-1);

    if (-1 == cdb32_read(self->find.cdb32, value.offset,
                         (cdb32_len_t)PyBytes_GET_SIZE(result),
                         (cdb32_key_t *)PyBytes_AS_STRING(result))) {
        /* LCOV_EXCL_START */

        Py_DECREF(result);
        return -1;

        /* LCOV_EXCL_STOP */
    }

    *value_ = result;
    return 0;
}
