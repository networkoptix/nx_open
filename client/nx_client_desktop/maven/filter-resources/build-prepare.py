import os, sys, posixpath, shutil
sys.path.insert(0, '${basedir}/../common')

target_dir = '${libdir}'

if '${platform}' == 'windows':
    for arch in ('x86', 'x64'):
        for config in ('debug', 'release'):
            target_plugins = os.path.join(target_dir, arch, 'bin', config, 'plugins')
            if os.path.exists(os.path.join(target_plugins, 'hw_decoding_conf.xml')):
                os.unlink(os.path.join(target_plugins, 'hw_decoding_conf.xml'))
            if '${quicksync}' == 'true':
                if not os.path.exists(target_plugins):
                    os.makedirs(target_plugins)
                shutil.copy2('${root.dir}/plugins/quicksyncdecoder/hw_decoding_conf.xml', target_plugins)
shutil.copy2('logo.png', 'resources/skin/logo.png')