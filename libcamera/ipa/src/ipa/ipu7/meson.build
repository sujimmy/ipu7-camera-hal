# SPDX-License-Identifier: CC0-1.0

ipa_name = 'ipa_ipu7'
ipa_igpu_name = 'ipa_ipu7_igpu'

ipu7_ipa_includes = [
    ipa_includes,
    libipa_includes,
]

ipu7_ipa_local_include = [
    libcamera_generated_ipa_headers,
]

ipu7_ipa_dependencies = [
    libcamera_private,
]

ipu7_ipa_sources = files([
    'ipu7.cpp',
    'IPAServer.cpp',
    'IPAServerThread.cpp',
    'server/CcaWorker.cpp',
    '../../libcamera/pipeline/ipu7/ipa/IPCCca.cpp',
])

ipu7_ipa_local_include += [
    'IPAHeader.h',
    'IPAServer.h',
    'IPAServerThread.h',
    'server/CcaWorker.h',
]

ipu7_ipa_includes += [
    'server/',
    '../../libcamera/pipeline/ipu7/ipa/',
    '../../libcamera/pipeline/ipu7/include/ia_imaging/',
]

ipu7_ipa_args = [
    '-DPAC_ENABLE',
]

ia_cca = cc.find_library('ia_cca', required : true)

ipu7_ipa_dependencies += [
    ia_cca,
]

igpu_ipa_includes = [
    ipa_includes,
    libipa_includes,
]

igpu_ipa_sources = files([
    'igpu.cpp',
    'IGPUIPAServer.cpp',
])

igpu_ipa_local_include = [
    libcamera_generated_ipa_headers,
]

igpu_ipa_local_include += [
    'IGPUIPAServer.h',
    'IGPUHeader.h',
]

igpu_ipa_dependencies = [
    libcamera_private,
]

igpu_mod = shared_module(ipa_igpu_name,
                        [igpu_ipa_sources, igpu_ipa_local_include],
                        name_prefix : '',
                        include_directories : ipu7_ipa_includes,
                        dependencies : igpu_ipa_dependencies,
                        link_with : libipa,
                        install : true,
                        install_dir : ipa_install_dir)

mod = shared_module(ipa_name,
                    [ipu7_ipa_sources, ipu7_ipa_local_include],
                    name_prefix : '',
                    include_directories : ipu7_ipa_includes,
                    dependencies : ipu7_ipa_dependencies,
                    cpp_args : ipu7_ipa_args,
                    link_with : libipa,
                    install : true,
                    install_dir : ipa_install_dir)

if ipa_sign_module
    custom_target(ipa_name + '.so.sign',
                  input : mod,
                  output : ipa_name + '.so.sign',
                  command : [ipa_sign, ipa_priv_key, '@INPUT@', '@OUTPUT@'],
                  install : false,
                  build_by_default : true)

    custom_target(ipa_igpu_name + '.so.sign',
                  input : igpu_mod,
                  output : ipa_igpu_name + '.so.sign',
                  command : [ipa_sign, ipa_priv_key, '@INPUT@', '@OUTPUT@'],
                  install : false,
                  build_by_default : true)
endif

ipa_names += ipa_name
ipa_names += ipa_igpu_name
