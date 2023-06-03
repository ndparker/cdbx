#!/usr/bin/env python
# -*- coding: ascii -*-
#
# Copyright 2016 - 2023
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
"""
Generate random cdbmake input
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

"""

import os as _os
import sys as _sys

fp = _sys.stdout
if bytes is not str:
    fp = fp.buffer  # pylint: disable = no-member

for j in range(int(_sys.argv[1])):
    klen = ord(_os.urandom(1).decode('latin-1'))
    vlen = (
        ord(_os.urandom(1).decode('latin-1')) << 8
        | ord(_os.urandom(1).decode('latin-1'))
    )
    fp.write(("+%s,%s:%s->%s\n" % (
        klen, vlen,
        _os.urandom(klen).decode('latin-1'),
        _os.urandom(vlen).decode('latin-1'),
    )).encode('latin-1'))
fp.write(b"\n")
