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
    'ipu7.cpp',
    'Frames.cpp',
    'CameraHal.cpp',
    'ParameterConverter.cpp',
    'Camera3AMetadata.cpp',
    'IntelAEStateMachine.cpp',
    'IntelAFStateMachine.cpp',
    'IntelAWBStateMachine.cpp',
# PNP_DEBUG_S
    'MockCameraHal.cpp',
# PNP_DEBUG_E
])

libcamera_sources += files([
    'modules/algowrapper/IntelFaceDetection.cpp',
    'modules/memory/chrome/BufferAllocator.cpp',
    'src/3a/AiqCore.cpp',
    'src/3a/AiqEngine.cpp',
    'src/3a/AiqResult.cpp',
    'src/3a/AiqResultStorage.cpp',
    'src/3a/AiqSetting.cpp',
    'src/3a/AiqUnit.cpp',
    'src/3a/AiqUtils.cpp',
    'src/3a/I3AControlFactory.cpp',
    'src/3a/MakerNote.cpp',
    'src/3a/LensManager.cpp',
    'src/3a/SensorManager.cpp',
    'src/3a/intel3a/Intel3AParameter.cpp',
    'src/core/BufferQueue.cpp',
    'src/core/CameraBuffer.cpp',
    'src/core/CameraBufferPool.cpp',
    'src/core/CameraDevice.cpp',
    'src/core/CameraContext.cpp',
    'src/core/CameraEvent.cpp',
    'src/core/CameraStream.cpp',
    'src/core/CaptureUnit.cpp',
    'src/core/DeviceBase.cpp',
    'src/core/LensHw.cpp',
    'src/core/IProcessingUnitFactory.cpp',
    'src/core/ProcessingUnit.cpp',
    'src/core/RequestThread.cpp',
    'src/core/SensorHwCtrl.cpp',
    'src/core/PSysDevice.cpp',
    'src/core/SofSource.cpp',
    'src/core/SwPostProcessUnit.cpp',
    'src/core/processingUnit/PipeManager.cpp',
    'src/core/processingUnit/IPipeManagerFactory.cpp',
    'src/core/processingUnit/PipeLine.cpp',
    'src/core/processingUnit/FaceStage.cpp',
    'src/core/processingUnit/CBStage.cpp',
    'src/core/processingUnit/ShareReferBufferPool.cpp',
    'src/core/processingUnit/PostProcessStage.cpp',
    'src/core/processingUnit/GPUPostStage.cpp',
    'src/core/processingUnit/IntelTNR7Stage.cpp',
    'modules/algowrapper/IntelICBM.cpp',
# PNP_DEBUG_S
    'src/core/FileSource.cpp',
    'src/platformdata/PnpDebugControl.cpp',
    'src/core/processingUnit/PipeManagerStub.cpp',
    'src/image_process/sw/ImageConverter.cpp',
    'src/image_process/sw/ImageScalerCore.cpp',
    'src/iutils/SwImageConverter.cpp',
# PNP_DEBUG_E
    'src/icbm/ICBMThread.cpp',
    'src/icbm/OnePunchIC2.cpp',
    'src/icbm/MemoryChain.cpp',
    'src/fd/FaceDetection.cpp',
    'src/fd/IFaceDetection.cpp',
    'src/fd/pvl/FaceDetectionPVL.cpp',
    'src/fd/facessd/FaceSSD.cpp',
    'src/image_process/PostProcessorBase.cpp',
    'src/image_process/PostProcessorCore.cpp',
# GPU_GLES_PROCESSOR_S
    'src/image_process/gpu/GPUPostProcessor.cpp',
# GPU_GLES_PROCESSOR_E
    'src/image_process/chrome/ImageProcessorCore.cpp',
    'src/iutils/CameraDump.cpp',
    'src/iutils/CameraLog.cpp',
    'src/iutils/ScopedAtrace.cpp',
    'src/iutils/Thread.cpp',
    'src/iutils/Trace.cpp',
    'src/iutils/Utils.cpp',
    'src/jpeg/EXIFMaker.cpp',
    'src/jpeg/EXIFMetaData.cpp',
    'src/jpeg/ExifCreater.cpp',
    'src/jpeg/JpegMaker.cpp',
    'src/jpeg/chrome/JpegEncoderCore.cpp',
    'src/platformdata/AiqInitData.cpp',
    'src/platformdata/CameraParserInvoker.cpp',
    'src/platformdata/CameraSensorsParser.cpp',
    'src/platformdata/JsonCommonParser.cpp',
    'src/platformdata/JsonParserBase.cpp',
    'src/platformdata/PlatformData.cpp',
    'src/platformdata/gc/GraphConfig.cpp',
    'src/platformdata/gc/GraphConfigManager.cpp',
    'src/platformdata/gc/GraphUtils.cpp',
    'src/scheduler/CameraScheduler.cpp',
    'src/scheduler/CameraSchedulerPolicy.cpp',
    'src/v4l2/MediaControl.cpp',
    'src/v4l2/SysCall.cpp',
    'src/v4l2/V4l2DeviceFactory.cpp',
    'src/v4l2/NodeInfo.cpp',
    'src/iutils/ModuleTags.cpp',
    'src/iutils/LogSink.cpp',
    'src/core/IpuPacAdaptor.cpp',
    'modules/ipu_desc/ptl/TerminalDescriptorAutogen.cpp',
    'modules/ipu_desc/ptl/CBLayoutUtils.cpp',
    'modules/ipu_desc/ptl/StaticGraphAutogen.cpp',
    'modules/ipu_desc/ptl/StaticGraphReaderAutogen.cpp',
    'modules/ipu_desc/ptl/GraphResolutionConfigurator.cpp',
    'modules/ipu_desc/ptl/GraphResolutionConfiguratorAutogen.cpp',
])
# for master branch build
ipu7_includes += include_directories(
    '.',
    'include',
    'include/api',
    'include/utils',
    'include/linux',
    'src',
    'src/fd',
    'src/fd/pvl',
    'src/fd/facessd',
    'src/platformdata',
    'src/platformdata/gc',
    'src/v4l2',
    'src/iutils',
    'src/core',
    'src/core/processingUnit',
    'src/scheduler',
    'src/3a/intel3a',
    'src/3a',
    'src/image_process',
    'src/image_process/sw',
    'src/image_process/gpu',
    'src/image_process/chrome',
    'src/jpeg/',
    'src/jpeg/chrome',
    'modules/memory',
    'modules/memory/chrome',
    'modules/algowrapper',
    'include/ia_imaging',
    'modules/ipu_desc',
    'modules/ipu_desc/ptl',
)

libcamera_args = [
    '-DCAL_BUILD',
    '-DHAVE_LINUX_OS',
    '-D__STDC_FORMAT_MACROS',
    '-DHAVE_PTHREADS',
    '-DHAVE_IA_TYPES',
    '-DHAVE_PRCTL',
    '-DCAMERA_HAL_DEBUG',
    '-DDUMP_IMAGE',
    '-DHAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL=0x10F',
    '-DMAX_PAC_BUFFERS=10',
    '-DFACE_DETECTION',
    '-DIPU7_SIMULATION',
    '-DIPU_SYSVER_ipu7',
    '-DIPU_SYSVER_ipu75',
    '-DPAC_ENABLE',
    '-DSTATIC_GRAPH_USE_IA_AIC_TYPES',
# PNP_DEBUG_S
    '-DPNP_DEBUG_TEST',
# PNP_DEBUG_E
]

libcamera_args += [
    '-Wno-shadow',
    '-Wno-sign-compare',
    '-Wno-missing-field-initializers',
    '-Wno-non-virtual-dtor',
    '-Wno-unused-parameter',
]

broxton_ia_pal = cc.find_library('broxton_ia_pal', required : true)
ia_aiqb_parser = cc.find_library('ia_aiqb_parser', required : true)
ia_cca = cc.find_library('ia_cca', required : true)
ia_cmc_parser = cc.find_library('ia_cmc_parser', required : true)
ia_exc = cc.find_library('ia_exc', required : true)
ia_log = cc.find_library('ia_log', required : true)
pvl_face_detection = cc.find_library('pvl_face_detection', required : true)
pvl_eye_detection = cc.find_library('pvl_eye_detection', required : true)
pvl_mouth_detection = cc.find_library('pvl_mouth_detection', required : true)
jpeg = cc.find_library('jpeg', required : true)
dl = cc.find_library('dl', required : true)
rt = cc.find_library('rt', required : true)
ia_aic = cc.find_library('ia_aic', required : true)
ia_aiq_v1 = cc.find_library('ia_aiq_v1', required : true)
expat = cc.find_library('expat', required : true)
jsoncpp = cc.find_library('jsoncpp', required : true)
facessd_cros = cc.find_library(
    'facessd_cros', required : true,
    dirs: '/build/nissa/build/share/cros_camera')

# internal libraries
libcamera_deps += [
    broxton_ia_pal,
    ia_aiqb_parser,
    ia_cca,
    ia_cmc_parser,
    ia_exc,
    ia_log,
    pvl_face_detection,
    pvl_eye_detection,
    pvl_mouth_detection,
    jpeg,
    dl,
    rt,
    facessd_cros,
    expat,
    ia_aic,
    ia_aiq_v1,
    jsoncpp,
]

# chrome packages
libchrome = dependency('libchrome', required : true)
libcros_camera = dependency('libcros_camera', required : true)
libcros_camera_android_deps = dependency('libcros_camera_android_deps', required : true)
libcros_config = dependency('libcros_config', required : true)
libmojo = dependency('libmojo', required : true)
libsync = dependency('libsync', required : true)
libyuv = dependency('libyuv', required : true)

libcamera_deps += [
    libchrome,
    libcros_camera,
    libcros_camera_android_deps,
    libcros_config,
    libmojo,
    libsync,
    libyuv,
]

# GPU_GLES_PROCESSOR_S
GLESv2 = cc.find_library('GLESv2', required : true)
OpenGL = cc.find_library('OpenGL', required : true)
EGL = cc.find_library('EGL', required : true)

libcamera_deps += [
  EGL,
  GLESv2,
  OpenGL,
]
# GPU_GLES_PROCESSOR_E

# set default as not enabling Sandbox
ipa_sandbox = false

if not ipa_sandbox
    libcamera_sources += files([
        'modules/algowrapper/IntelCca.cpp',
    ])
else
    libcamera_args += [
        '-DIPA_SANDBOXING',
    ]

    subdir('ipa')
endif
