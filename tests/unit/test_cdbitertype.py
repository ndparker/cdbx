# -*- coding: ascii -*-
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

=============================
 Tests for CDB iterator type
=============================

Tests for CDB iterator type.
"""
__author__ = u"Andr\xe9 Malo"

import tempfile as _tempfile
import weakref as _weakref

from pytest import raises

import cdbx as _cdbx

# pylint: disable = consider-using-with, pointless-statement


def test_closed():
    """bail if closed"""
    make = _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True)
    make.add('foo', 'bar')
    cdb = make.commit()

    obj = cdb.items()
    for _ in obj:
        cdb.close()
        break

    with raises(IOError):
        list(obj)


def test_weakref():
    """weakref handling"""
    make = _cdbx.CDB.make(_tempfile.TemporaryFile(), close=True)
    make.add('foo', 'bar')
    cdb = make.commit()

    obj = iter(cdb)
    proxy = _weakref.proxy(obj)
    assert list(proxy) == [b'foo']
    assert list(proxy) == []
    del obj

    with raises(ReferenceError):
        list(proxy)
