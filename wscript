#!/usr/bin/env python
from waflib.extras import autowaf as autowaf
import re

# Variables for 'waf dist'
APPNAME = 'bs2b.lv2'
VERSION = '0.1.0'

# Mandatory variables
top = '.'
out = 'build'

default_lv2dir = '/usr/lib/lv2'


def options(opt):
    opt.load('compiler_c')
    autowaf.set_options(opt)
    opt.add_option('--lv2dir', action='store', dest='lv2dir', type='string',
                   default=default_lv2dir,
                   help='Plugin install path [default: %s]' % default_lv2dir)


def configure(conf):
    conf.load('compiler_c')
    autowaf.configure(conf)
    autowaf.set_c99_mode(conf)
    autowaf.display_header('Bs2b Configuration')

    if not autowaf.is_child():
        autowaf.check_pkg(conf, 'lv2', uselib_store='LV2')

    autowaf.check_pkg(conf, 'libbs2b', uselib_store='BS2B',
                      atleast_version='3.1.0', mandatory=True)

    conf.env.LV2DIR = conf.options.lv2dir
    autowaf.display_msg(conf, 'LV2 bundle directory', conf.env.LV2DIR)
    print('')


def build(bld):
    bundle = 'bs2b.lv2'

    # Make a pattern for shared objects without the 'lib' prefix
    module_pat = re.sub('^lib', '', bld.env.cshlib_PATTERN)
    module_ext = module_pat[module_pat.rfind('.'):]

    # Build manifest.ttl by substitution (for portable lib extension)
    bld(features     = 'subst',
        source       = 'manifest.ttl.in',
        target       = '%s/%s' % (bundle, 'manifest.ttl'),
        install_path = '${LV2DIR}/%s' % bundle,
        LIB_EXT      = module_ext)

    # Copy other data files to build bundle (build/eg-amp.lv2)
    for i in ['bs2b.ttl', 'bs2b_presets.ttl']:
        bld(features     = 'subst',
            is_copy      = True,
            source       = i,
            target       = '%s/%s' % (bundle, i),
            install_path = '${LV2DIR}/%s' % bundle)

    # Use LV2 headers from parent directory if building as a sub-project
    includes = None
    if autowaf.is_child:
        includes = '../..'

    # Build plugin library
    obj = bld(features     = 'c cshlib',
              source       = 'plugin.c',
              name         = 'bs2b',
              target       = '%s/bs2b' % bundle,
              install_path = '${LV2DIR}/%s' % bundle,
              uselib       = 'LV2 BS2B',
              includes     = includes)
    obj.env.cshlib_PATTERN = module_pat
