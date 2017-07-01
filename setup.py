#!/usr/bin/env python
# $yfId: btree/trunk/setup.py 82 2017-07-02 08:44:28Z futatuki $

import sys
import os
import os.path
import platform
from distutils.core import setup
from Cython.Distutils.extension import Extension
from Cython.Distutils import build_ext
from distutils.command.build import build as _build
from distutils.cmd import Command

class build(_build):
    sub_commands = [('pre_build', None)] + _build.sub_commands

class pre_build(Command):
    description = "run pre-build jobs"
    user_options = []
    boolean_options = []
    help_opthons = []
    def initialize_options(self):
        return
    def finalize_options(self):
        return
    def run(self):
        os.system(  'sed -e s/@DEBUG@/0/ Btree.pxd.in > Btree.pxd &&'
                    'sed -e s/@DEBUG@/0/ Btree.pyx.in > Btree.pyx' )
extensions = [
        Extension('Btree', ['btree.c', 'Btree.pyx'], 
                    define_macros=[('GENERIC_LIB', '1')])]

setup(name='Btree',
    version='0.1.0',
    description= 'C implement of B+ tree Class interface',
    author='FUTATSUKI Yasuhito',
    author_email='futatuki@yf.bsdclub.org',
    license="BSD 2 clause",
    package_data = { '': ['*.pxd'] },
    ext_modules = extensions,
    cmdclass = {'pre_build' : pre_build,
                'build'     : build,
                'build_ext' : build_ext}
)
