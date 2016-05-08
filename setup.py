#!/usr/bin/env python
# -*- coding: ascii -*-
#
# Copyright 2016
# Andr\xe9 Malo or his licensors, as applicable
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from _setup import run


def setup(args=None, _manifest=0):
    """ Main setup function """
    from _setup.ext import Extension

    return run(
        script_args=args,
        manifest_only=_manifest,
        ext=[
            Extension('cdbx._cdb', [
                "cdbx/_cdb32/buffer.c",
                "cdbx/_cdb32/buffer_put.c",
                "cdbx/_cdb32/byte_copy.c",
                "cdbx/_cdb32/byte_diff.c",
                "cdbx/_cdb32/cdb.c",
                "cdbx/_cdb32/cdb_hash.c",
                "cdbx/_cdb32/cdb_make.c",
                "cdbx/_cdb32/error.c",
                "cdbx/_cdb32/seek_set.c",
                "cdbx/_cdb32/str_len.c",
                "cdbx/_cdb32/uint32_pack.c",
                "cdbx/_cdb32/uint32_unpack.c",

                "cdbx/main.c",
                "cdbx/cdbtype.c",
                "cdbx/cdbmaker.c",
                "cdbx/cdbkeyiter.c",
                "cdbx/util.c",
            ], depends=[
                "cdbx/_cdb32/buffer.h",
                "cdbx/_cdb32/byte.h",
                "cdbx/_cdb32/cdb.h",
                "cdbx/_cdb32/cdb_make.h",
                "cdbx/_cdb32/error.h",
                "cdbx/_cdb32/seek.h",
                "cdbx/_cdb32/str.h",
                "cdbx/_cdb32/uint32.h",
                "cdbx/_cdb32/readwrite.h",

                "cdbx/cdbx.h",
            ], include_dirs=[
                "cdbx",
                "cdbx/_cdb32",
            ])
        ],
    )


def manifest():
    """ Create List of packaged files """
    return setup((), _manifest=1)


if __name__ == '__main__':
    setup()
