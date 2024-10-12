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

========================================
 CDBx - CDB Reimplementation for Python
========================================

CDBx - CDB reimplementation for Python.
"""
__author__ = u"Andr\xe9 Malo"
__license__ = "Apache License, Version 2.0"
__version__ = '0.2.4'
__all__ = ['CDB', 'CDBMaker']

try:
    from cdbx._cdb import __version__ as _c_version
except ImportError:
    _c_version = None
if _c_version != __version__:
    raise ImportError(
        "Could not import cdbx C extension version %r. Found: %r"
        % (__version__, _c_version)
    )
del _c_version

# pylint: disable = wrong-import-position
from cdbx._cdb import CDB, CDBMaker
