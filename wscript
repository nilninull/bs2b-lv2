#!/usr/bin/env python

from waflib.extras import autowaf

APPNAME = 'bs2b.lv2'
VERSION = '1.0.0'
top     = '.'
out     = 'build'

# Release variables
title        = 'BS2B.lv2'
uri          = 'https://github.com/nilninull/bs2b-lv2'
# dist_pattern = ''
post_tags    = ['LV2', 'BS2B.lv2']

def options(opt):
    opt.load('compiler_c')
    opt.load('lv2')

def configure(conf):
    conf.load('compiler_c', cache=True)
    conf.load('lv2', cache=True)
    conf.load('autowaf', cache=True)
    autowaf.set_c_lang(conf, 'c99')

    conf.check_pkg('lv2 >= 1.16.0', uselib_store='LV2')
    conf.run_env.append_unique('LV2_PATH', [conf.build_path('lv2')])
    autowaf.display_summary(conf, {'LV2 bundle directory': conf.env.LV2DIR})

    autowaf.check_pkg(conf, 'libbs2b', uselib_store='BS2B',
                      atleast_version='3.1.0', mandatory=True)

def build(bld):
    bundle = 'bs2b.lv2'

    # Build manifest.ttl by substitution (for portable lib extension)
    bld(features     = 'subst',
        source       = 'manifest.ttl.in',
        target       = '%s/%s' % (bundle, 'manifest.ttl'),
        install_path = '${LV2DIR}/%s' % bundle,
        LIB_EXT      = bld.env.LV2_LIB_EXT)

    # Copy other data files to build bundle
    for i in ['bs2b.ttl', 'bs2b_presets.ttl']:
        bld(features     = 'subst',
            is_copy      = True,
            source       = i,
            target       = '%s/%s' % (bundle, i),
            install_path = '${LV2DIR}/%s' % bundle)

    # Build plugin library
    obj = bld(features     = 'c cshlib lv2lib',
              source       = 'plugin.c',
              name         = 'bs2b',
              target       = '%s/bs2b' % bundle,
              install_path = '${LV2DIR}/%s' % bundle,
              uselib       = 'LV2 BS2B')
