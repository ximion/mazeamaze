# Syntalos Modules

module_lib_def_tmpl = files('module-lib-def.toml.in')
ln_exe = find_program('ln')
modules_enabled = get_option('modules')

# Developer modules
subdir('devel.clock')
subdir('devel.datasource')
subdir('devel.datasst')
subdir('devel.pyooptest')

# Display / Aux modules
subdir('canvas')
subdir('table')

# Video
subdir('videorecorder')
subdir('videotransform')
subdir('camera-generic')

if 'camera-tis' in modules_enabled
    subdir('camera-tis')
endif
if 'camera-flir' in modules_enabled
    subdir('camera-flir')
endif
if 'camera-ueye' in modules_enabled
    subdir('camera-ueye')
endif
if 'miniscope' in modules_enabled
    subdir('miniscope')
endif
subdir('triled-tracker')

# Python & Customization
subdir('firmata-io')
subdir('firmata-userctl')
subdir('pyscript')

# Electrophysiology
if 'intan-rhx' in modules_enabled
    subdir('intan-rhx')
endif
subdir('traceplot')

# Misc
subdir('runcmd')
subdir('deeplabcut-live')

# Examples
subdir('example-py')
