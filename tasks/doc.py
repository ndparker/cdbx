# -*- encoding: ascii -*-
"""
Doc Tasks
~~~~~~~~~

"""

import os as _os

import invoke as _invoke

from . import compile as _compile


@_invoke.task(_compile.compile)
def userdoc(ctx):
    """Create userdocs"""
    _sphinx(ctx, ctx.doc.sphinx.build, ctx.doc.sphinx.source, ctx.doc.userdoc)


@_invoke.task(userdoc, default=True)
def doc(ctx):  # pylint: disable = unused-argument
    """Create docs (userdoc + apidoc)"""


@_invoke.task()
def website(ctx):
    """Create website"""
    version = None
    with open(ctx.shell.native('%(package)s/__init__.py' % ctx), 'rb') as fp:
        init = fp.read().decode('latin-1').splitlines()
    for line in init:
        if line.startswith('__version__'):
            version = line.split('=', 1)[1].strip()
            if version.startswith(("'", '"')):
                version = version[1:-1].strip()
            break
    else:
        raise RuntimeError("Version not found")

    ctx.shell.rm_rf(ctx.doc.website.source)
    ctx.shell.cp_r(
        ctx.doc.sphinx.source, _os.path.join(ctx.doc.website.source, 'src')
    )
    ctx.shell.rm_rf(ctx.doc.website.target)
    ctx.shell.mkdir_p(ctx.doc.website.target)

    dlfile = _os.path.join(
        ctx.doc.website.source, 'src', 'website_download.txt'
    )
    with open(dlfile, 'rb') as fp:
        download = fp.read().decode('latin-1')

    ixfile = _os.path.join(ctx.doc.website.source, 'src', 'index.txt')
    with open(ixfile, 'rb') as fp:
        index = fp.read().decode('latin-1').splitlines(True)

    with open(ixfile, 'wb') as fp:
        for line in index:
            if line.startswith('.. placeholder: Download'):
                line = download
            fp.write(line.encode('latin-1'))

    _sphinx(
        ctx,
        ctx.shell.native(_os.path.join(ctx.doc.website.source, 'build')),
        ctx.shell.native(_os.path.join(ctx.doc.website.source, 'src')),
        ctx.shell.native(ctx.doc.website.target),
    )
    ctx.shell.rm(_os.path.join(ctx.doc.website.target, '.buildinfo'))


def _sphinx(ctx, build, source, target):
    """Run sphinx"""
    apidoc = ctx.shell.frompath('sphinx-apidoc')
    if apidoc is None:
        raise RuntimeError("shpinx-apidoc not found")
    sphinx = ctx.shell.frompath('sphinx-build')
    if sphinx is None:
        raise RuntimeError("sphinx-build not found")

    with ctx.shell.root_dir():
        ctx.run(
            ctx.c(
                r'''
            %(apidoc)s -f --private -stxt -e -o %(source)s/apidoc %(package)s
        ''',
                **dict(
                    apidoc=apidoc,
                    source=source,
                    package=ctx.package,
                )
            ),
            env=dict(
                SPHINX_APIDOC_OPTIONS='members,undoc-members,special-members',
            ),
            echo=True,
        )
        ctx.run(
            ctx.c(
                r'''
            %(sphinx)s -a -d %(doctrees)s -b html %(source)s %(target)s
        ''',
                **dict(
                    sphinx=sphinx,
                    doctrees=_os.path.join(build, 'doctrees'),
                    source=source,
                    target=target,
                )
            ),
            echo=True,
        )
