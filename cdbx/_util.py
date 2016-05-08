# -*- coding: ascii -*-
r"""
:Copyright:

 Copyright 2014 - 2016
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

================
 Misc Utilities
================

Misc utilities.
"""
if __doc__:
    # pylint: disable = redefined-builtin
    __doc__ = __doc__.encode('ascii').decode('unicode_escape')
__author__ = r"Andr\xe9 Malo".encode('ascii').decode('unicode_escape')
__docformat__ = "restructuredtext en"


def find_public(space):
    """
    Determine all public names in space

    If the space contains an ``__all__`` sequence, a copy is returned (as a
    list). Otherwise, all symbol names not starting with an underscore are
    listed.

    :Parameters:
      `space` : ``dict``
        Name space to inspect

    :Return: List of public names
    :Rtype: ``list``
    """
    if '__all__' in space:
        return list(space['__all__'])
    return [key for key in space.keys() if not key.startswith('_')]
