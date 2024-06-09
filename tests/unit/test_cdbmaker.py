# -*- coding: ascii -*-
u"""
:Copyright:

 Copyright 2021 - 2024
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
import tempfile as _tempfile
import weakref as _weakref

from pytest import raises

import cdbx as _cdbx

# pylint: disable = consider-using-with, pointless-statement


def fix_path(name):
    """Find fixture"""
    return _os.path.join(_os.path.dirname(__file__), 'fixtures', name)


def fix(name):
    """Load fixture"""
    with open(fix_path(name), 'rb') as fp:
        return fp.read()


def test_closed():
    """Bail on closed maker"""
    make = _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True)
    make.close()

    with raises(IOError):
        make.commit()

    with raises(IOError):
        make.fileno()

    make.close()  # noop

    fp = _tempfile.TemporaryFile()
    try:
        make = _cdbx.CDB.make(fp, close=True)
    finally:
        fp.close()

    with raises(IOError):
        make.commit()
    with raises(IOError):
        make.commit()


def test_commit_args():
    """add() args error handling"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True)
    ) as make:
        with raises(TypeError):
            make.commit(lah='luh')


def test_add_args():
    """add() args error handling"""
    with closing(
        _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True)
    ) as make:
        with raises(TypeError):
            make.add(lah='luh')

        with raises(TypeError):
            make.add(object(), object())

        with raises(IOError):
            make.add('doh', 'duh')


def test_fileno():
    """fileno() works as expected"""
    fp = _tempfile.TemporaryFile()
    with closing(_cdbx.CDB.make(fp, close=True)) as make:
        assert make.fileno() == fp.fileno()


def test_fd():
    """fd handling"""
    with _tempfile.TemporaryFile() as fp:
        make = _cdbx.CDB.make(fp.fileno())
        make.add('foo', 'bar')
        cdb = make.commit()

    assert cdb['foo'] == b'bar'
    cdb.close()

    with raises(IOError):
        cdb['foo']

    fp = _tempfile.TemporaryFile()
    _cdbx.CDB.make(fp.fileno(), close=True).close()
    with raises(IOError):
        fp.close()


def test_filename(tmpdir):
    """filename handling"""
    fname = _os.path.join(str(tmpdir), 'foo.cdb')
    make = _cdbx.CDB.make(fname)

    assert _os.path.isfile(fname)
    make.close()
    assert not _os.path.isfile(fname)


def test_new_badfile():
    """__new__() args error handling"""
    with raises((TypeError, AttributeError)):
        _cdbx.CDB.make(object())


def test_weakref():
    """weakref handling"""
    make = _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True)
    proxy = _weakref.proxy(make)

    proxy.add('foo', 'bar')
    proxy.close()

    with raises(IOError):
        proxy.add('baz', 'zopp')

    del make
    with raises(ReferenceError):
        proxy.add('duh', 'dah')
