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

========================
 Version Representation
========================

Version representation.
"""
if __doc__:
    # pylint: disable = redefined-builtin
    __doc__ = __doc__.encode('ascii').decode('unicode_escape')
__author__ = r"Andr\xe9 Malo".encode('ascii').decode('unicode_escape')
__docformat__ = "restructuredtext en"


try:
    unicode
except NameError:
    py3 = True
    unicode = str  # pylint: disable = redefined-builtin, invalid-name
else:
    py3 = False


class Version(tuple):
    """
    Container representing the package version

    :IVariables:
      `major` : ``int``
        The major version number

      `minor` : ``int``
        The minor version number

      `patch` : ``int``
        The patch level version number

      `is_dev` : ``bool``
        Is it a development version?

      `revision` : ``int``
        Internal revision
    """
    _str = "(unknown)"

    def __new__(cls, versionstring, is_dev, revision):
        """
        Construction

        :Parameters:
          `versionstring` : ``str``
            The numbered version string (like ``"1.1.0"``)
            It should contain at least three dot separated numbers

          `is_dev` : ``bool``
            Is it a development version?

          `revision` : ``int``
            Internal revision

        :Return: New version instance
        :Rtype: `Version`
        """
        # pylint: disable = W0613

        tup = []
        versionstring = versionstring.strip()
        isuni = not(py3) and isinstance(versionstring, unicode)
        strs = []
        if versionstring:
            for item in versionstring.split('.'):
                try:
                    item = int(item)
                    strs.append(str(item))
                except ValueError:
                    if py3:
                        strs.append(item)
                    elif isuni:
                        strs.append(item.encode('utf-8'))
                    else:
                        try:
                            item = item.decode('ascii')
                            strs.append(item.encode('ascii'))
                        except UnicodeError:
                            try:
                                item = item.decode('utf-8')
                                strs.append(item.encode('utf-8'))
                            except UnicodeError:
                                strs.append(item)
                                item = item.decode('latin-1')
                tup.append(item)
        while len(tup) < 3:
            tup.append(0)
        self = tuple.__new__(cls, tup)
        self._str = ".".join(strs)  # pylint: disable = W0212
        return self

    def __init__(self, versionstring, is_dev, revision):
        """
        Initialization

        :Parameters:
          `versionstring` : ``str``
            The numbered version string (like ``1.1.0``)
            It should contain at least three dot separated numbers

          `is_dev` : ``bool``
            Is it a development version?

          `revision` : ``int``
            Internal revision
        """
        # pylint: disable = W0613

        super(Version, self).__init__()
        self.major, self.minor, self.patch = self[:3]
        self.is_dev = bool(is_dev)
        self.revision = int(revision)

    def __repr__(self):
        """
        Create a development string representation

        :Return: The string representation
        :Rtype: ``str``
        """
        if py3:
            return "%s.%s('%s', is_dev=%r, revision=%r)" % (
                self.__class__.__module__,
                self.__class__.__name__,
                (
                    self._str
                    .replace('\\', '\\\\')
                    .encode('unicode_escape')
                    .decode('ascii')
                    .replace("'", "\\'")
                ),
                self.is_dev,
                self.revision,
            )
        else:
            return "%s.%s(%r, is_dev=%r, revision=%r)" % (
                self.__class__.__module__,
                self.__class__.__name__,
                self._str,
                self.is_dev,
                self.revision,
            )

    def __str__(self):
        """
        Create a version like string representation

        :Return: The string representation
        :Rtype: ``str``
        """
        return "%s%s" % (
            self._str,
            ("", ".dev%d" % self.revision)[self.is_dev],
        )

    def __unicode__(self):
        """
        Create a version like unicode representation

        :Return: The unicode representation
        :Rtype: ``unicode``
        """
        u = lambda x: x.decode('ascii')  # pylint: disable = invalid-name

        return u("%s%s") % (
            u(".").join(map(unicode, self)),
            (u(""), u(".dev%d") % self.revision)[self.is_dev],
        )

    def __bytes__(self):
        """
        Create a version like bytes representation (utf-8 encoded)

        :Return: The bytes representation
        :Rtype: ``bytes``
        """
        return str(self).encode('utf-8')
