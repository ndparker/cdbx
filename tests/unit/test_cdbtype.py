# -*- coding: utf-8 -*-
u"""
:Copyright:

 Copyright 2016 - 2024
 Andr\xe9 Malo or his licensors, as applicable

:License:

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

====================
 Tests for cdbx.CDB
====================

Tests for cdbx.CDB
"""
__author__ = u"Andr\xe9 Malo"

from contextlib import closing
import os as _os
import sys as _sys
import tempfile as _tempfile
import weakref as _weakref

from pytest import raises

import cdbx as _cdbx

from .. import _util as _test

# pylint: disable = consider-using-with, pointless-statement


def fix_path(name):
    """Find fixture"""
    return _os.path.join(_os.path.dirname(__file__), 'fixtures', name)


def fix(name):
    """Load fixture"""
    with open(fix_path(name), 'rb') as fp:
        return fp.read()


def test_closed():
    """Bail on closed DB"""
    cdb = _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    cdb.close()

    with raises(IOError):
        cdb['foo']

    with raises(IOError):
        cdb.get('bar')

    with raises(IOError):
        cdb.items('baz')

    with raises(IOError):
        cdb.keys()

    with raises(IOError):
        list(cdb)

    with raises(IOError):
        len(cdb)

    with raises(IOError):
        cdb.fileno()

    with raises(IOError):
        'foo' in cdb

    with raises(IOError):
        cdb.__contains__('foo')

    cdb.close()  # noop


def test_get_args():
    """get() args error handling"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    ) as cdb:
        with raises(TypeError):
            cdb.get(nope='wrong')

        with raises(RuntimeError) as e:
            cdb.get('foo', all=_test.badbool)
        assert e.value.args == ('yoyo',)


def test_items_args():
    """items() args error handling"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    ) as cdb:
        with raises(TypeError):
            cdb.items(nope='wrong')

        with raises(RuntimeError) as e:
            cdb.items(all=_test.badbool)
        assert e.value.args == ('yoyo',)


def test_keys_args():
    """keys() args error handling"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    ) as cdb:
        with raises(TypeError):
            cdb.keys(nope='wrong')

        with raises(RuntimeError) as e:
            cdb.keys(all=_test.badbool)
        assert e.value.args == ('yoyo',)


def test_make_args():
    """make() args error handling"""
    with raises(TypeError):
        _cdbx.CDB.make(duh='dah')

    with raises(RuntimeError) as e:
        _cdbx.CDB.make(12, close=_test.badbool)
    assert e.value.args == ('yoyo',)

    with raises(RuntimeError) as e:
        _cdbx.CDB.make(12, mmap=_test.badbool)
    assert e.value.args == ('yoyo',)


def test_new_args():
    """__new__() args error handling"""
    with raises(TypeError):
        _cdbx.CDB(duh='dah')

    with raises(RuntimeError) as e:
        _cdbx.CDB(12, close=_test.badbool)
    assert e.value.args == ('yoyo',)

    with raises(RuntimeError) as e:
        _cdbx.CDB(12, mmap=_test.badbool)
    assert e.value.args == ('yoyo',)

    with raises(OverflowError):
        _cdbx.CDB(-3)

    with raises(OverflowError):
        _cdbx.CDB(_sys.maxsize + 1)


def test_fileno():
    """fileno() works as expected"""
    fp = _tempfile.TemporaryFile()
    with closing(_cdbx.CDB.make(fp, close=True).commit()) as cdb:
        assert cdb.fileno() == fp.fileno()


def test_getitem_badstring():
    """getitem() bails on bad string"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    ) as cdb:
        with raises(TypeError):
            # pylint: disable = expression-not-assigned
            cdb[object()]

        with raises(ValueError):
            cdb[u'Андрей']


def test_contains_badstring():
    """__contains__() bails on bad string"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    ) as cdb:
        with raises(TypeError):
            # pylint: disable = expression-not-assigned
            object() in cdb

        with raises(ValueError):
            u'Андрей' in cdb


def test_new_badfile():
    """__new__() args error handling"""
    with raises((TypeError, AttributeError)):
        _cdbx.CDB(object())


def test_fd():
    """fd handling"""
    fp = _tempfile.TemporaryFile()
    make = _cdbx.CDB.make(fp)
    make.add('foo', 'bar')
    make.commit()

    cdb = _cdbx.CDB(fp.fileno())
    assert cdb['foo'] == b'bar'
    cdb.close()

    with raises(IOError):
        cdb['foo']

    _cdbx.CDB(fp.fileno(), close=True).close()
    with raises(IOError):
        fp.close()

    fp = _tempfile.TemporaryFile()
    _cdbx.CDB.make(fp).commit()
    cdb = _cdbx.CDB(fp.fileno(), close=True)
    fp.close()
    with raises(OSError):
        cdb.close()


def test_bad_mmap():
    """Fake bad mmap module"""

    class Fake(object):
        """Hell raiser"""

        @property
        def mmap(self):
            """well..."""
            raise RuntimeError("Huh")

    with _test.patched_import('mmap', Fake()):
        fp = _tempfile.TemporaryFile()
        try:
            _cdbx.CDB.make(fp).commit()

            with raises(RuntimeError) as e:
                _cdbx.CDB(fp, mmap=True)
            assert e.value.args == ('Huh',)

            _cdbx.CDB(fp, mmap=None).close()
        finally:
            fp.close()


def test_new_filename(tmpdir):
    """__new__() filename handling"""
    fname = _os.path.join(str(tmpdir), 'cdbtype_new_filename.cdb')
    cdb = _cdbx.CDB.make(fname).commit()

    assert _os.path.isfile(fname)
    cdb.close()
    assert _os.path.isfile(fname)

    make = _cdbx.CDB.make(fname)
    _os.unlink(fname)
    with raises(OSError):
        make.close()

    with raises(IOError):
        _cdbx.CDB(fname)


def test_weakref():
    """weakref handling"""
    cdb = _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True).commit()
    proxy = _weakref.proxy(cdb)

    assert 'foo' not in proxy
    cdb.close()

    with raises(IOError):
        'foo' not in proxy

    del cdb
    with raises(ReferenceError):
        'foo' not in proxy
