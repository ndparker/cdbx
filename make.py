#!/usr/bin/env python
# -*- coding: ascii -*-
#
# Copyright 2006 - 2016
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
===============
 Build targets
===============

Build targets.
"""
__author__ = "Andr\xe9 Malo"
__author__ = getattr(__author__, 'decode', lambda x: __author__)('latin-1')
__docformat__ = "restructuredtext en"

import errno as _errno
import os as _os
import re as _re
import sys as _sys

from _setup import dist
from _setup import shell
from _setup import make
from _setup import term
from _setup.make import targets

if _sys.version_info[0] == 3:
    py3 = 1
    cfgread = dict(encoding='utf-8')
    def textopen(*args):
        return open(*args, **cfgread)
    exec ("def reraise(*e): raise e[1].with_traceback(e[2])")
else:
    py3 = 0
    try:
        True
    except NameError:
        exec ("True = 1; False = 0")
    textopen = open
    cfgread = {}
    exec ("def reraise(*e): raise e[0], e[1], e[2]")


class Target(make.Target):
    def init(self):
        self.dirs = {
            'lib': '.',
            'docs': 'docs',
            'tests': 'tests',
            'coverage': 'docs/coverage',
            'apidoc': 'docs/apidoc',
            'userdoc': 'docs/userdoc',
            'userdoc_source': 'docs/_userdoc',
            'userdoc_build': 'docs/_userdoc/_build',
            'website': 'dist/website',
            '_website': '_website',  # source dir
            'dist': 'dist',
            'build': 'build',
            'ebuild': '_pkg/ebuilds',
        }
        libpath = shell.native(self.dirs['lib'])
        if libpath != _sys.path[0]:
            while libpath in _sys.path:
                _sys.path.remove(libpath)
            _sys.path.insert(0, libpath)

        self.ebuild_files = {
            'cdbx-alpha.ebuild.in':
                'cdbx-%(VERSION)s_alpha%(REV)s.ebuild',
            # 'cdbx.ebuild.in': 'cdbx-%(VERSION)s.ebuild',
        }


Manifest = targets.Manifest


class Distribution(targets.Distribution):
    def init(self):
        self._dist = 'dist'
        self._ebuilds = '_pkg/ebuilds'
        self._changes = 'docs/CHANGES'


class Check(Target):
    """ Check the python code """
    NAME = "check"
    DEPS = ["compile-quiet"]

    def run(self):
        from _setup.dev import analysis
        term.green('Linting cdbx sources...')
        res = analysis.pylint('pylintrc', 'cdbx')
        if res == 2:
            make.warn('pylint not found', self.NAME)


class Test(Target):
    """ Test the code """
    NAME = "test"
    DEPS = ["tox-test"]


class ToxTest(Target):
    """ Run the tests via tox """
    NAME = "tox-test"
    DEPS = ["compile-quiet", "MANIFEST"]

    def run(self):
        if shell.spawn('tox'):
            raise RuntimeError('tests failed')

    def clean(self, scm, dist):
        term.green("Removing coverage files...")
        shell.rm_rf(self.dirs['coverage'])
        shell.rm('.coverage')
        shell.rm_rf('.tox')
        shell.rm_rf('.cache')


class Compile(Target):
    """ Compile the python code """
    NAME = "compile"
    # DEPS = None

    def run(self):
        import setup

        _old_argv = _sys.argv
        try:
            _sys.argv = ['setup.py', '-q', 'build']
            if not self.HIDDEN:
                _sys.argv.remove('-q')
            setup.setup()
            if 'java' not in _sys.platform.lower():
                _sys.argv = [
                    'setup.py', '-q', 'install_lib', '--install-dir',
                    shell.native(self.dirs['lib']),
                    '--optimize', '2',
                ]
                if not self.HIDDEN:
                    _sys.argv.remove('-q')
                setup.setup()
        finally:
            _sys.argv = _old_argv

        for name in shell.files("%s/cdbx" % self.dirs['lib'], '*.py'):
            self.compile(name)
        term.write("%(ERASE)s")

        term.green("All files successfully compiled.")

    def compile(self, name):
        path = shell.native(name)
        term.write("%(ERASE)s%(BOLD)s>>> Compiling %(name)s...%(NORMAL)s",
                   name=name)
        from distutils import util
        try:
            from distutils import log
        except ImportError:
            util.byte_compile([path], verbose=0, force=True)
        else:
            log.set_verbosity(0)
            util.byte_compile([path], force=True)

    def clean(self, scm, dist):
        term.green("Removing python byte code...")
        for name in shell.dirs('.', '__pycache__'):
            shell.rm_rf(name)
        for name in shell.files('.', '*.py[co]'):
            shell.rm(name)
        for name in shell.files('.', '*$py.class'):
            shell.rm(name)

        term.green("Removing c extensions...")
        for name in shell.files('.', '*.so'):
            shell.rm(name)
        for name in shell.files('.', '*.pyd'):
            shell.rm(name)

        shell.rm_rf(self.dirs['build'])


class CompileQuiet(Compile):
    NAME = "compile-quiet"
    HIDDEN = True

    def clean(self, scm, dist):
        pass


class Doc(Target):
    """ Build the docs (api + user) """
    NAME = "doc"
    DEPS = ['apidoc', 'userdoc']


class ApiDoc(Target):
    """ Build the API docs """
    NAME = "apidoc"
    DEPS = ['compile-quiet']

    def run(self):
        from _setup.dev import apidoc
        apidoc.epydoc(
            prepend=[
                shell.native(self.dirs['lib']),
            ],
            env={'EPYDOC_INSPECTOR': '1'}
        )

    def clean(self, scm, dist):
        if scm:
            term.green("Removing apidocs...")
            shell.rm_rf(self.dirs['apidoc'])


class UserDoc(Target):
    """ Build the user docs """
    NAME = "userdoc"
    #DEPS = None

    def run(self):
        from _setup.dev import userdoc
        userdoc.sphinx(
            build=shell.native(self.dirs['userdoc_build']),
            source=shell.native(self.dirs['userdoc_source']),
            target=shell.native(self.dirs['userdoc']),
        )

    def clean(self, scm, dist):
        if scm:
            term.green("Removing userdocs...")
            shell.rm_rf(self.dirs['userdoc'])
        shell.rm_rf(self.dirs['userdoc_build'])


class Website(Target):
    """ Build the website """
    NAME = "website"
    DEPS = ["apidoc"]

    def run(self):
        from _setup.util import SafeConfigParser as parser
        parser = parser()
        parser.read('package.cfg', **cfgread)
        strversion = parser.get('package', 'version.number')
        shortversion = tuple(map(int, strversion.split('.')[:2]))

        shell.rm_rf(self.dirs['_website'])
        shell.cp_r(
            self.dirs['userdoc_source'],
            _os.path.join(self.dirs['_website'], 'src')
        )
        shell.rm_rf(_os.path.join(self.dirs['_website'], 'build'))
        shell.rm_rf(self.dirs['website'])
        _os.makedirs(self.dirs['website'])
        filename = _os.path.join(
            self.dirs['_website'], 'src', 'website_download.txt'
        )
        fp = textopen(filename)
        try:
            download = fp.read()
        finally:
            fp.close()
        filename = _os.path.join(self.dirs['_website'], 'src', 'index.txt')
        fp = textopen(filename)
        try:
            indexlines = fp.readlines()
        finally:
            fp.close()

        fp = textopen(filename, 'w')
        try:
            for line in indexlines:
                if line.startswith('.. placeholder: Download'):
                    line = download
                fp.write(line)
        finally:
            fp.close()

        shell.cp_r(
            self.dirs['apidoc'],
            _os.path.join(self.dirs['website'], 'doc-%d.%d' % shortversion)
        )
        shell.cp_r(
            self.dirs['apidoc'],
            _os.path.join(
                self.dirs['_website'], 'src', 'doc-%d.%d' % shortversion
            )
        )
        fp = textopen(_os.path.join(
            self.dirs['_website'], 'src', 'conf.py'
        ), 'a')
        try:
            fp.write("\nepydoc = dict(cdbx=%r)\n" % (
                _os.path.join(
                    shell.native(self.dirs['_website']),
                    "src",
                    "doc-%d.%d" % shortversion,
                ),
            ))
            fp.write("\nexclude_trees.append(%r)\n" %
                "doc-%d.%d" % shortversion
            )
        finally:
            fp.close()
        from _setup.dev import userdoc
        userdoc.sphinx(
            build=shell.native(_os.path.join(self.dirs['_website'], 'build')),
            source=shell.native(_os.path.join(self.dirs['_website'], 'src')),
            target=shell.native(self.dirs['website']),
        )
        shell.rm(_os.path.join(self.dirs['website'], '.buildinfo'))

    def clean(self, scm, dist):
        if scm:
            term.green("Removing website...")
            shell.rm_rf(self.dirs['website'])
        shell.rm_rf(self.dirs['_website'])


class PreCheck(Target):
    """ Run clean, doc, check """
    NAME = "precheck"
    DEPS = ["clean", "doc", "check", "test"]


class SVNRelease(Target):
    """ Release current version """
    #NAME = "release"
    DEPS = None

    def run(self):
        self._check_committed()
        self._update_versions()
        self._tag_release()
        self.runner('dist', seen={})

    def _tag_release(self):
        """ Tag release """
        from _setup.util import SafeConfigParser as parser
        parser = parser()
        parser.read('package.cfg', **cfgread)
        strversion = parser.get('package', 'version.number')
        isdev = parser.getboolean('package', 'version.dev')
        revision = parser.getint('package', 'version.revision')
        version = strversion
        if isdev:
            version += '.dev%d' % (revision,)
        trunk_url = self._repo_url()
        if not trunk_url.endswith('/trunk'):
            rex = _re.compile(r'/branches/\d+(?:\.\d+)*\.[xX]$').search
            match = rex(trunk_url)
            if not match:
                make.fail("Not in trunk or release branch!")
            found = match.start(0)
        else:
            found = -len('/trunk')
        release_url = trunk_url[:found] + '/releases/' + version

        svn = shell.frompath('svn')
        shell.spawn(
            svn, 'copy', '-m', 'Release version ' + version, '--',
            trunk_url, release_url,
            echo=True,
        )

    def _update_versions(self):
        """ Update versions """
        self.runner('revision', 'version', seen={})
        svn = shell.frompath('svn')
        shell.spawn(svn, 'commit', '-m', 'Pre-release: version update',
            echo=True
        )

    def _repo_url(self):
        """ Determine URL """
        from xml.dom import minidom
        svn = shell.frompath('svn')
        info = minidom.parseString(
            shell.spawn(svn, 'info', '--xml', stdout=True)
        )
        try:
            url = info.getElementsByTagName('url')[0]
            text = []
            for node in url.childNodes:
                if node.nodeType == node.TEXT_NODE:
                    text.append(node.data)
        finally:
            info.unlink()
        return (''.decode('ascii')).join(text).encode('utf-8')

    def _check_committed(self):
        """ Check if everything is committed """
        if not self._repo_url().endswith('/trunk'):
            rex = _re.compile(r'/branches/\d+(?:\.\d+)*\.[xX]$').search
            match = rex(self._repo_url())
            if not match:
                make.fail("Not in trunk or release branch!")
        svn = shell.frompath('svn')
        lines = shell.spawn(svn, 'stat', '--ignore-externals',
            stdout=True, env=dict(_os.environ, LC_ALL='C'),
        ).splitlines()
        for line in lines:
            if line.startswith('X'):
                continue
            make.fail("Uncommitted changes!")


class GitRelease(Target):
    """ Release current version """
    #NAME = "release"
    DEPS = None

    def run(self):
        self._check_committed()
        self._update_versions()
        self._tag_release()
        self.runner('dist', seen={})

    def _tag_release(self):
        """ Tag release """
        from _setup.util import SafeConfigParser as parser
        parser = parser()
        parser.read('package.cfg', **cfgread)
        strversion = parser.get('package', 'version.number')
        isdev = parser.getboolean('package', 'version.dev')
        revision = parser.getint('package', 'version.revision')
        version = strversion
        if isdev:
            version += '.dev%d' % (revision,)
        git = shell.frompath('git')
        shell.spawn(
            git, 'tag', '-a', '-m', 'Release version ' + version, '--',
            version,
            echo=True,
        )

    def _update_versions(self):
        """ Update versions """
        self.runner('revision', 'version', seen={})
        git = shell.frompath('git')
        shell.spawn(git, 'commit', '-a', '-m', 'Pre-release: version update',
            echo=True
        )

    def _check_committed(self):
        """ Check if everything is committed """
        git = shell.frompath('git')
        lines = shell.spawn(git, 'branch', '--color=never',
            stdout=True, env=dict(_os.environ, LC_ALL='C')
        ).splitlines()
        for line in lines:
            if line.startswith('*'):
                branch = line.split(None, 1)[1]
                break
        else:
            make.fail("Could not determine current branch.")
        if branch != 'master':
            rex = _re.compile(r'^\d+(?:\.\d+)*\.[xX]$').match
            match = rex(branch)
            if not match:
                make.fail("Not in master or release branch.")

        lines = shell.spawn(git, 'status', '--porcelain',
            stdout=True, env=dict(_os.environ, LC_ALL='C'),
        )
        if lines:
            make.fail("Uncommitted changes!")


class Release(GitRelease):
    NAME = "release"
    #DEPS = None


class SVNRevision(Target):
    """ Insert the svn revision into all relevant files """
    #NAME = "revision"
    #DEPS = None

    def run(self):
        revision = self._revision()
        self._revision_cfg(revision)

    def _revision(self):
        """ Find SVN revision """
        rev = shell.spawn(shell.frompath('svnversion'), '.', stdout=True)
        rev = rev.strip()
        if ':' in rev:
            rev = rev.split(':')[1]
        try:
            rev = int(rev)
        except ValueError:
            try:
                rev = int(rev[:-1])
            except ValueError:
                make.fail("No clean revision found (%s)" % rev)
        return rev

    def _revision_cfg(self, revision):
        """ Modify version in package.cfg """
        filename = 'package.cfg'
        fp = textopen(filename)
        try:
            initlines = fp.readlines()
        finally:
            fp.close()
        fp = textopen(filename, 'w')
        replaced = False
        try:
            for line in initlines:
                if line.startswith('version.revision'):
                    line = 'version.revision = %d\n' % (revision,)
                    replaced = True
                fp.write(line)
        finally:
            fp.close()
        assert replaced, "version.revision not found in package.cfg"


class SimpleRevision(Target):
    """ Update the revision number and insert into all relevant files """
    #NAME = "revision"
    #DEPS = None

    def run(self):
        self._revision_cfg()

    def _revision_cfg(self):
        """ Modify version in package.cfg """
        filename = 'package.cfg'
        fp = textopen(filename)
        try:
            initlines = fp.readlines()
        finally:
            fp.close()
        fp = textopen(filename, 'w')
        revision, replaced = None, False
        try:
            for line in initlines:
                if line.startswith('version.revision'):
                    if revision is None:
                        revision = int(line.split('=', 1)[1].strip() or 0, 10)
                        revision += 1
                    line = 'version.revision = %d\n' % (revision,)
                    replaced = True
                fp.write(line)
        finally:
            fp.close()
        assert replaced, "version.revision not found in package.cfg"

GitRevision = SimpleRevision


class Revision(GitRevision):
    """ Insert the revision into all relevant files """
    NAME = "revision"
    #DEPS = None


class Version(Target):
    """ Insert the program version into all relevant files """
    NAME = "version"
    #DEPS = None

    def run(self):
        from _setup.util import SafeConfigParser as parser
        parser = parser()
        parser.read('package.cfg', **cfgread)
        strversion = parser.get('package', 'version.number')
        isdev = parser.getboolean('package', 'version.dev')
        revision = parser.getint('package', 'version.revision')

        self._version_init(strversion, isdev, revision)
        self._version_userdoc(strversion, isdev, revision)
        self._version_download(strversion, isdev, revision)
        self._version_changes(strversion, isdev, revision)

        parm = {'VERSION': strversion, 'REV': revision}
        for src, dest in self.ebuild_files.items():
            src = "%s/%s" % (self.dirs['ebuild'], src)
            dest = "%s/%s" % (self.dirs['ebuild'], dest % parm)
            term.green("Creating %(name)s...", name=dest)
            shell.cp(src, dest)

    def _version_init(self, strversion, isdev, revision):
        """ Modify version in __init__ """
        filename = _os.path.join(self.dirs['lib'], 'cdbx',
                                 '__init__.py')
        fp = textopen(filename)
        try:
            initlines = fp.readlines()
        finally:
            fp.close()
        fp = textopen(filename, 'w')
        replaced = False
        try:
            for line in initlines:
                if line.startswith('__version__'):
                    line = '__version__ = (%r, %r, %r)\n' % (
                        strversion, isdev, revision
                    )
                    replaced = True
                fp.write(line)
        finally:
            fp.close()
        assert replaced, "__version__ not found in __init__.py"

    def _version_changes(self, strversion, isdev, revision):
        """ Modify version in changes """
        filename = _os.path.join(shell.native(self.dirs['docs']), 'CHANGES')
        if isdev:
            strversion = "%s.dev%d" % (strversion, revision)
        fp = textopen(filename)
        try:
            initlines = fp.readlines()
        finally:
            fp.close()
        fp = textopen(filename, 'w')
        try:
            for line in initlines:
                if line.rstrip() == "Changes with version":
                    line = "%s %s\n" % (line.rstrip(), strversion)
                fp.write(line)
        finally:
            fp.close()

    def _version_userdoc(self, strversion, isdev, revision):
        """ Modify version in userdoc """
        filename = _os.path.join(self.dirs['userdoc_source'], 'conf.py')
        shortversion = '.'.join(strversion.split('.')[:2])
        longversion = strversion
        if isdev:
            longversion = "%s.dev%d" % (strversion, revision)
        fp = textopen(filename)
        try:
            initlines = fp.readlines()
        finally:
            fp.close()
        replaced = 0
        fp = textopen(filename, 'w')
        try:
            for line in initlines:
                if line.startswith('version'):
                    line = 'version = %r\n' % shortversion
                    replaced |= 1
                elif line.startswith('release'):
                    line = 'release = %r\n' % longversion
                    replaced |= 2
                fp.write(line)
        finally:
            fp.close()
        assert replaced & 3 != 0, "version/release not found in conf.py"

    def _version_download(self, strversion, isdev, revision):
        """ Modify version in website download docs """
        filename = _os.path.join(
            self.dirs['userdoc_source'], 'website_download.txt'
        )
        dllines, VERSION, PATH = [], strversion, ''
        if isdev:
            oldstable = []
            hasstable = False
            try:
                fp = textopen(filename)
            except IOError:
                e = _sys.exc_info()[1]
                if e.args[0] != _errno.ENOENT:
                    raise
            else:
                try:
                    for line in fp:
                        if line.startswith('.. begin stable'):
                            hasstable = True
                        oldstable.append(line)
                finally:
                    fp.close()
            if hasstable:
                dllines = oldstable
            else:
                VERSION = "%s.dev%d" % (strversion, revision)
                PATH='dev/'
        newdev = []
        fp = textopen(filename + '.in')
        try:
            if dllines:
                for line in fp:
                    if newdev:
                        newdev.append(line)
                        if line.startswith('.. end dev'):
                            break
                    elif line.startswith('.. begin dev'):
                        newdev.append(line)
                else:
                    raise AssertionError("Incomplete dev marker")
            else:
                dllines = fp.readlines()
        finally:
            fp.close()
        instable, indev = [], []
        fp = textopen(filename, 'w')
        try:
            for line in dllines:
                if instable:
                    instable.append(line)
                    if line.startswith('.. end stable'):
                        if not isdev:
                            res = (''.join(instable)
                                .replace('@@VERSION@@', strversion)
                                .replace('@@PATH@@', '')
                            )
                        elif not hasstable:
                            res = ''
                        else:
                            res = ''.join(instable)
                        fp.write(res)
                        instable = []
                elif indev:
                    indev.append(line)
                    if line.startswith('.. end dev'):
                        if isdev:
                            if newdev:
                                indev = newdev
                            fp.write(''.join(indev)
                                .replace('@@DEVVERSION@@', "%s.dev%d" % (
                                    strversion, revision
                                ))
                                .replace('@@PATH@@', 'dev/')
                            )
                        else:
                            fp.write(''.join([indev[0], indev[-1]]))
                        indev = []
                elif line.startswith('.. begin stable'):
                    instable.append(line)
                elif line.startswith('.. begin dev'):
                    indev.append(line)
                elif isdev and hasstable:
                    fp.write(line)
                else:
                    fp.write(line
                        .replace('@@VERSION@@', VERSION)
                        .replace('@@PATH@@', PATH)
                    )
        finally:
            fp.close()

    def clean(self, scm, dist):
        """ Clean versioned files """
        if scm:
            term.green("Removing generated ebuild files")
            for name in shell.files(self.dirs['ebuild'], '*.ebuild'):
                shell.rm(name)


make.main(name=__name__)
