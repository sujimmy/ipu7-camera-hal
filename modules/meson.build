#
#  Copyright (C) 2024 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

libcamera_sources += files([
    'ipu_desc/ptl/TerminalDescriptorAutogen.cpp',
    'ipu_desc/ptl/CBLayoutUtils.cpp',
    'ipu_desc/ptl/StaticGraphAutogen.cpp',
    'ipu_desc/ptl/StaticGraphReaderAutogen.cpp',
    'ipu_desc/ptl/GraphResolutionConfigurator.cpp',
    'ipu_desc/ptl/GraphResolutionConfiguratorAutogen.cpp',
])
# for master branch build
includes += include_directories(
    '.',
    'ipu_desc',
    'ipu_desc/ptl',
)

if get_option('android_platform').contains('cros')
    libcamera_sources += files([
        'memory/libcamera/BufferAllocator.cpp',
    ])

    includes += include_directories(
        'memory',
    )
endif
