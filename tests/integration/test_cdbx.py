# -*- coding: ascii -*-
r"""
:Copyright:

 Copyright 2016
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
if __doc__:
    # pylint: disable = redefined-builtin
    __doc__ = __doc__.encode('ascii').decode('unicode_escape')
__author__ = r"Andr\xe9 Malo".encode('ascii').decode('unicode_escape')
__docformat__ = "restructuredtext en"

import os as _os
import tempfile as _tempfile

import cdbx as _cdbx

from pytest import raises


def fix_path(name):
    """ Find fixture """
    return _os.path.join(_os.path.dirname(__file__), 'fixtures', name)


def fix(name):
    """ Load fixture """
    with open(fix_path(name), 'rb') as fp:
        return fp.read()


def test_empty():
    """ Create minimum number of keys """
    fp = _tempfile.TemporaryFile()
    try:
        cdb = _cdbx.CDB.make(fp).commit()
        assert len(cdb) == 0
        fp.seek(0)
        assert fp.read() == fix('empty.cdb')
    finally:
        fp.close()


def test_empty_key():
    """ Create empty keys and value """
    fp = _tempfile.TemporaryFile()
    try:
        cdb = _cdbx.CDB.make(fp)
        for _ in range(10):
            cdb.add("", "")
        cdb = cdb.commit()
        assert len(cdb) == 1
    finally:
        fp.close()


def test_get():
    """ Get method """
    with _tempfile.TemporaryFile() as fp:
        cdb = _cdbx.CDB.make(fp)
        cdb.add('a', 'bc')
        cdb.add('def', 'ghij')
        cdb.add('def', 'klmno')
        cdb.add('a', 'xxy')
        cdb.add('b', 'sakdhgjksghf')
        cdb = cdb.commit()
        assert len(cdb) == 3
        assert cdb['a'] == b'bc'
        assert cdb.get('a') == b'bc'
        assert cdb.get('a', all=True) == [b'bc', b'xxy']
        assert cdb['def'] == b'ghij'
        assert cdb.get('def') == b'ghij'
        assert cdb.get('def', all=True) == [b'ghij', b'klmno']

        with raises(KeyError):
            cdb['c']  # pylint: disable = pointless-statement
        assert cdb.get('c') is None
        assert cdb.get('c', all=True) is None
        assert cdb.get('c', default='lla') == 'lla'
        assert cdb.get('c', default='lolo', all=True) == 'lolo'

        assert cdb['b'] == b'sakdhgjksghf'
        assert cdb.get('b') == b'sakdhgjksghf'
        assert cdb.get('b', all=True) == [b'sakdhgjksghf']


def test_random():
    """ Test random mapping """
    fp = _tempfile.TemporaryFile()
    try:
        cdb = _cdbx.CDB.make(fp)
        keys = {}
        dump = []
        numkeys = 0
        with open(fix_path('random.txt'), 'rb') as inp:
            while True:
                c = inp.read(1)
                if c == b'\n':
                    break
                elif c != b'+':
                    raise AssertionError('Format!')
                klen = int(''.join([
                    x.decode('ascii') for x in iter(lambda: inp.read(1), b',')
                ]))
                vlen = int(''.join([
                    x.decode('ascii') for x in iter(lambda: inp.read(1), b':')
                ]))
                key = inp.read(klen)
                assert inp.read(2) == b'->'
                value = inp.read(vlen)
                assert inp.read(1) == b'\n'

                cdb.add(key, value)
                dump.append((key, value))
                numkeys += 1
                keys.setdefault(key, value)
        cdb = cdb.commit()
        fp.seek(0)
        assert fp.read() == fix('random.cdb')
        assert len(cdb) == len(keys)
        assert len(list(cdb.keys(all=True))) == numkeys

        for key, value in keys.items():
            assert cdb[key] == value

        assert list(sorted(cdb.keys())) == list(sorted(keys))

        dump.reverse()
        for tup in cdb.items(all=True):
            assert tup == dump.pop()

        nokey = b''
        while nokey in keys:
            nokey += _os.urandom(1)

        assert nokey not in cdb

        with raises(KeyError) as e:
            cdb[nokey]  # pylint: disable = pointless-statement
        assert e.value.args == (nokey,)
    finally:
        fp.close()
