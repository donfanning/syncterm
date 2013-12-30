#!/usr/bin/env python

# If you get an "error: Python.h: No such file or directory" on running
# this then you need to install the python-dev package.

from distutils.core import setup, Extension
import sys

if sys.platform == "win32":
    ext = Extension("cryptlib_py",
                    sources=["bindings/python.c"],
                    library_dirs=['../binaries'],
                    libraries=['cl32'])
else:
    ext = Extension("cryptlib_py",
                    sources=["bindings/python.c"],
                    library_dirs=['..'],
                    libraries=['cl'])

setup(name="cryptlib_py", ext_modules=[ext])
