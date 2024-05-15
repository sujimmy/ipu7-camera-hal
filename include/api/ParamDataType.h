/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Filename: ParamDateType.h
 */

#pragma once

#include <vector>
#include <set>
#include <stdint.h>

namespace icamera {

/***************Start of Camera Basic Data Structure ****************************/
// VIRTUAL_CHANNEL_S
/**
 * \struct vc_info_t: Define the virtual channel information for the device
 */
typedef struct {
    int total_num; /**< the total camera number of virtual channel. 0: the virtual channel is disabled */
    int sequence; /**< the current camera's sequence in all the virtual channel cameras */
    int group; /**< the virtual channel group id */
} vc_info_t;
// VIRTUAL_CHANNEL_E

/**
 * \struct device_info_t: Define each camera basic information
 */
typedef struct {
    int facing;
    int orientation;
    int device_version;
    const char* name; /**< Sensor name */
    const char* description; /**< Sensor description */
} device_info_t;

/**
 * Basic definition will be inherited by more complicated structure.
 * MUST be all "int" in this structure.
 */
typedef struct {
    int width;
    int height;
} camera_resolution_t;

/**
 * \struct stream_t: stream basic info
 *
 * \note
 *   MUST use int if new member added.
 */
typedef struct {
    int format; /**< stream format refer to v4l2 definition
                   https://linuxtv.org/downloads/v4l-dvb-apis/pixfmt.html */
    int width;  /**< image width */
    int height; /**< image height */
    int field;  /**< refer to v4l2 definition
                   https://linuxtv.org/downloads/v4l-dvb-apis/field-order.html#v4l2-field */

    /*
     * The buffer geometry introduction.
     * The YUV image is formed with Y:Luma and UV:Chroma. And there are
     * two kinds of styles for YUV format: planar and packed.
     *
     *   YUV420:NV12
     *
     *            YUV420(720x480) sampling
     *
     *       |<----width+padding=alignedBpl----->|
     *     Y *-------*-------*-------*-------*....-----
     *       |                               |   :  ^
     *       |   # UV            #           |   :  |
     *       |                               |   :  |
     *       *-------*-------*-------*-------*....  |
     *       |                               |   :  |
     *       |   #               #           |   :  |
     *       |                               |   :  |
     *       *-------*-------*-------*-------*.... (height * 3 / 2)
     *       |                               |   :  |
     *       |   #               #           |   :  |
     *       |                               |   :  |
     *       *-------*-------*-------*-------*....  |
     *       |                               |   :  |
     *       |   #               #           |   :  |
     *       |                               |   :  v
     *       *-------*-------*-------*-------*....-----
     *
     *         The data stored in memory
     *          ____________w___________ .....
     *         |Y0|Y1                   |    :
     *         |                        |    :
     *         h                        h    :
     *         |                        |    :
     *         |                        |    :
     *         |________________________|....:
     *         |U|V|U|V                 |    :
     *        h/2                      h/2   :
     *         |____________w___________|....:
     *
     *       bpp = 12
     *       bpl = width;
     *       stride = align64(bpl):
     *
     *   YUV422:YUY2
     *
     *           YUV422(720x480) sampling
     *
     *       |<--(width*2)+padding=alignedBpl-->|
     *   YUV *#----*#-----*#-----*#-----*#....-----
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#.... (height)
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#....  |
     *       *#----*#-----*#-----*#-----*#....-----
     *
     *         The data stored in memory
     *          ____________w___________ .....
     *         |Y0|Cb|Y1|Cr             |    :
     *         |                        |    :
     *         |                        |    :
     *         |                        |    :
     *         h                        h    :
     *         |                        |    :
     *         |                        |    :
     *         |                        |    :
     *         |____________w___________|....:
     *
     *       bpp = 16
     *       bpl = width * bpp / 8 = width * 2;
     *       stride = align64(bpl):
     *
     *       Note: The stride defined in HAL is same as aligned bytes per line.
     */
    int stride; /**< stride = aligned bytes per line */
    int size;   /**< real buffer size */

    int id;      /**< Id that is filled by HAL. */
    int memType; /**< buffer memory type filled by app, refer to
                    https://linuxtv.org/downloads/v4l-dvb-apis/io.html */

    /**
     * The maximum number of buffers the HAL device may need to have dequeued at
     * the same time. The HAL device may not have more buffers in-flight from
     * this stream than this value.
     */
    uint32_t max_buffers;

    int usage;          /**<The usage of this stream defined in camera_stream_usage_t. */
    int streamType;     /**<The stream type of this stream defined in camera_stream_type_t. */
    int orientation;    /**<The orientation of this stream. [0, 90, 180, 270] */
} stream_t;

typedef std::vector<stream_t> stream_array_t;

/**
 * \struct stream_config_t: stream configuration info
 *
 * Contains all streams info in this configuration.
 */
typedef struct {
    int num_streams;   /**< number of streams in this configuration */
    stream_t* streams; /**< streams list */
    /**
     * The operation mode of the streams in this configuration. It should be one of the value
     * defined in camera_stream_configuration_mode_t.
     * The HAL uses this mode as an indicator to set the stream property (e.g.,
     * camera_stream->max_buffers) appropriately. For example, if the configuration is
     * CAMERA_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE, the HAL may want to set aside more
     * buffers for batch mode operation (see camera.control.availableHighSpeedVideoConfigurations
     * for batch mode definition).
     */
    uint32_t operation_mode;
} stream_config_t;

/**
 * \struct camera_buffer_flags_t: Specify a buffer's properties.
 *
 * The buffer's properties can be one of them or combined with some of them.
 */
typedef enum {
    BUFFER_FLAG_DMA_EXPORT = 1 << 0,
    BUFFER_FLAG_INTERNAL = 1 << 1,
    BUFFER_FLAG_SW_READ = 1 << 2,
    BUFFER_FLAG_SW_WRITE = 1 << 3,
    BUFFER_FLAG_NO_FLUSH = 1 << 4,
} camera_buffer_flags_t;

/**
 * \struct camera_buffer_t: camera buffer info
 *
 * camera buffer is used to carry device frames. Application allocate buffer structure,
 * according to memory type to allocate memory and queue to device.
 */
typedef struct {
    stream_t s;         /**< stream info */
    void* addr;         /**< buffer addr for userptr and mmap memory mode */
    int index;          /**< buffer index, filled by HAL. it is used for qbuf and dqbuf in order */
    int64_t sequence;   /**< buffer sequence, filled by HAL, to record buffer dqueue sequence from
                           device */
    int dmafd;          /**< buffer dmafd for DMA import and export mode */
    int flags;          /**< buffer flags, its type is camera_buffer_flags_t, used to specify buffer
                           properties */
    uint64_t timestamp; /**< buffer timestamp, it's a time reference measured in nanosecond */
    uint32_t frameNumber;   /**< buffer frameNumber, it's an id of buffer */
    void *priv;         /**< used to pass private data */
    uint64_t reserved;  /**< reserved for future */
} camera_buffer_t;

/**
 * camera_stream_type_t:
 *
 * The type of the camera stream, which defines whether the camera HAL device
 * is the producer or the consumer for that stream, and how the buffers of that
 * stream relate to the other streams.
 */
typedef enum {
    /**
     * This stream is an output stream; the camera HAL device will be responsible to
     * fill the buffers of this stream with newly captured or reprocessed image data.
     */
    CAMERA_STREAM_OUTPUT = 0,

    /**
     * This stream is an input stream; the camera HAL device will be responsible
     * to read buffers from this stream and to send them through the camera
     * processing pipeline, as if the buffer was a newly captured image from
     * the imager.
     *
     * The pixel format for an input stream can be any format reported by
     * camera.scaler.availableInputOutputFormatsMap. The pixel format of the
     * output stream used to produce the reprocessing data may be any format
     * reported by camera.scaler.availableStreamConfigurations. The supported
     * inputoutput stream combinations depends on the camera device capabilities.
     * See camera.scaler.availableInputOutputFormatsMap for stream map details.
     *
     * This kind of stream is generally used to reprocess data into higher
     * quality images (that otherwise would cause a frame rate performance loss),
     * or to do off-line reprocessing.
     * The typical use cases are OPAQUE (typically ZSL) and YUV reprocessing.
     */
    CAMERA_STREAM_INPUT = 1,

    /**
     * This stream can be used for input and output. Typically, the stream is
     * used as an output stream, but occasionally one already-filled buffer may
     * be sent back to the HAL device for reprocessing.
     *
     * This kind of stream is generally meant for Zero Shutter Lag (ZSL)
     * features, where copying the captured image from the output buffer to the
     * reprocessing input buffer would be expensive.
     *
     * Note that the HAL will always be reprocessing data it produced.
     *
     */
    CAMERA_STREAM_BIDIRECTIONAL = 2,

    /**
     * Total number of framework-defined stream types
     */
    CAMERA_NUM_STREAM_TYPES

} camera_stream_type_t;

/**
 * camera_stream_usage_t:
 *
 * The type of the camera stream, which defines whether the camera HAL device
 * is the producer or the consumer for that stream, and how the buffers of that
 * stream relate to the other streams.
 */
typedef enum {
    /**
     * This stream is an output stream for preview;
     */
    CAMERA_STREAM_PREVIEW = 0,

    /**
     * This stream is an output stream for VIDEO CAPTURE;
     */
    CAMERA_STREAM_VIDEO_CAPTURE,

    /**
     * This stream is an output stream for STILL IMAGE CAPTURE;
     */
    CAMERA_STREAM_STILL_CAPTURE,

    /**
     * This stream is an output stream for Application processing which is accessed by CPU;
     */
    CAMERA_STREAM_APP,

    /**
     * This stream is an output stream for Opaque RAW reprocess.
     */
    CAMERA_STREAM_OPAQUE_RAW,

    CAMERA_STREAM_MAX,
} camera_stream_usage_t;

/**
 * camera_stream_configuration_mode_t:
 *
 * This defines the general operation mode for the HAL (for a given stream configuration), where
 * modes besides NORMAL have different semantics, and usually the generality of the APIs are
 * limited in exchange for higher performance in some particular area.
 */
typedef enum {
    /**
     * Normal stream configuration operation mode.
     * This is the default camera operation mode, where all semantics of HAL APIs and metadata
     * controls apply.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_NORMAL = 0,

    /**
     * CONSTRAINED_HIGH_SPEED is the special constrained high speed operation mode for devices
     * that do not support high speed output in NORMAL mode.
     * To support this configuration mode, camera.control.availableHighSpeedVideoConfigurations
     * should be implemented and CONSTRAINED_HIGH_SPEED should be reported in
     * camera.request.availableCapabilities.
     * All streams in this configuration mode operate at high speed mode and have different
     * characteristics and limitations to achieve high speed output. The NORMAL mode can also be
     * used for high speed output, if the HAL supports high speed output while satisfying all the
     * semantics of HAL APIs and metadata controls. It is recommended for the HAL to support high
     * speed output in NORMAL mode (by advertising the high speed FPS ranges in
     * camera.control.aeAvailableTargetFpsRanges) if possible.
     *
     * This mode has below limitations/requirements:
     *
     *   1. The HAL must support up to 2 streams with sizes reported by
     *       camera.control.availableHighSpeedVideoConfigurations.
     *   2. In this mode, the HAL is expected to output up to 120fps or higher. It must
     *       support the targeted FPS range and resolution configurations reported by
     *       camera.control.availableHighSpeedVideoConfigurations.
     *   3. To achieve efficient high speed streaming, the HAL may have to aggregate multiple
     *       frames together and send the batch to camera device for processing there the request
     *       controls are same for all the frames in this batch (batch mode). The HAL must
     *       support the max batch size. And the max batch size requirements are defined by
     *       camera.control.availableHighSpeedVideoConfigurations.
     *   4. The HAL will override {aeMode, awbMode, afMode} to {ON, ON, CONTINUOUS_VIDEO}.
     *       All post-processing block mode controls must be overridden to be FAST. Therefore, no
     *       manual control of capture and post-processing parameters is possible. All other
     *       controls operate the same as when camera.control.mode == AUTO.
     *       This means that all other camera.control.* fields must continue to work, such as
     *           camera.control.aeTargetFpsRange
     *           camera.control.aeExposureCompensation
     *           camera.control.aeLock
     *           camera.control.awbLock
     *           camera.control.effectMode
     *           camera.control.aeRegions
     *           camera.control.afRegions
     *           camera.control.awbRegions
     *           camera.control.afTrigger
     *           camera.control.aePrecaptureTrigger
     *       Outside of camera.control.*, the following controls must work:
     *           camera.flash.mode (TORCH mode only, automatic flash for still capture will not
     *                                           work since aeMode is ON)
     *           camera.lens.opticalStabilizationMode (if it is supported)
     *           camera.scaler.cropRegion
     *           camera.statistics.faceDetectMode (if it is supported)
     *
     * Note: The high speed mode is not completely supported yet.
     *       1) Now the HAL supports up to 60fps@1080p.
     *       2) The static metadata camera.control.availableHighSpeedVideoConfigurations should be
     *           implemented.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_CONSTRAINED_HIGH_SPEED = 1,

    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_AUTO is a configurable mode, but not a real
     * mode in HAL. The user uses this mode to allow the HAL selects appropriate config mode
     * internally, so it should NOT be regarded as specific ConfigMode, but operation mode only.
     *
     * TuningModes used in AUTO mode depends on ConfigMode the HAL selects.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_AUTO,
    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_HDR is used to select PSYS pipeline,
     * TuningMode and MediaCtlConfig HDR pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_HDR,
    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_ULL is used to select PSYS pipeline,
     * TuningMode and MediaCtlConfig ULL pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_ULL,
    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_HLC is used to select PSYS pipeline,
     * TuningMode and MediaCtlConfig HLC pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_HLC,
    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_CUSTOM_AIC is used to select PSYS pipeline,
     * TuningMode and MediaCtlConfig CUSTOM_AIC pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_CUSTOM_AIC,

    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_VIDEO_LL is used to select PSYS pipeline,
     * TuningMode and MediaCtlConfig Video LL pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_VIDEO_LL,

    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_STILL_CAPTURE is used to select PSYS pipeline,
     * Create only still pipe
     * TuningMode and MediaCtlConfig still pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_STILL_CAPTURE,

    /**
     * CAMERA_STREAM_CONFIGURATION_MODE_HDR2 is used to select PSYS pipeline,
     * TuningMode and MediaCtlConfig HDR2 pipe.
     */
    CAMERA_STREAM_CONFIGURATION_MODE_HDR2,

    CAMERA_STREAM_CONFIGURATION_MODE_END
} camera_stream_configuration_mode_t;

/***************End of Camera Basic Data Structure ****************************/

/*******************Start of Camera Parameters Definition**********************/
/**
 * \enum camera_features: camera supported features.
 */
typedef enum {
    MANUAL_EXPOSURE,      /**< Allow user to control exposure time and ISO manually */
    MANUAL_WHITE_BALANCE, /**< Allow user to control AWB mode, cct range, and gain */
    IMAGE_ENHANCEMENT,    /**< Sharpness, Brightness, Contrast, Hue, Saturation */
    NOISE_REDUCTION,      /**< Allow user to control NR mode and NR level */
    SCENE_MODE,           /**< Allow user to control scene mode */
    WEIGHT_GRID_MODE,     /**< Allow user to control custom weight grid mode */
    PER_FRAME_CONTROL,    /**< Allow user to control most of parameters for each frame */
    ISP_CONTROL,          /**< Allow user to control low level ISP features */
    INVALID_FEATURE
} camera_features;
typedef std::vector<camera_features> camera_features_list_t;

/**
 * \struct camera_range_t: Used to specify the range info for something like FPS.
 */
typedef struct {
    float min;
    float max;
} camera_range_t;
typedef std::vector<camera_range_t> camera_range_array_t;

/**
 * \enum camera_ae_mode_t: Used to control how AE works.
 */
typedef enum {
    AE_MODE_AUTO,   /**< */
    AE_MODE_MANUAL, /**< */
    AE_MODE_MAX     /**< Invalid AE mode, any new mode should be added before this */
} camera_ae_mode_t;

typedef enum { AE_STATE_NOT_CONVERGED, AE_STATE_CONVERGED } camera_ae_state_t;

/**
 * \enum camera_antibanding_mode_t: Used to control antibanding mode.
 */
typedef enum {
    ANTIBANDING_MODE_AUTO, /**< Auto detect the flicker frequency. */
    ANTIBANDING_MODE_50HZ, /**< Specify the flicker frequency to 50Hz. */
    ANTIBANDING_MODE_60HZ, /**< Specify the flicker frequency to 60Hz. */
    ANTIBANDING_MODE_OFF,  /**< Do not try to remove the flicker. */
} camera_antibanding_mode_t;

/**
 * \enum camera_scene_mode_t: Used to control scene mode.
 *
 * Different scene mode may have different WB effects or different exposure behavior.
 */
typedef enum {
    SCENE_MODE_AUTO,
    SCENE_MODE_HDR,
    SCENE_MODE_ULL,
    SCENE_MODE_HLC,
    SCENE_MODE_NORMAL,
    SCENE_MODE_CUSTOM_AIC,
    SCENE_MODE_VIDEO_LL,
    SCENE_MODE_STILL_CAPTURE,
    SCENE_MODE_HDR2,
    SCENE_MODE_MAX
} camera_scene_mode_t;

/**
 * \struct camera_ae_exposure_time_range_t: Provide supported exposure time range info per scene
 * mode.
 */
typedef struct {
    camera_scene_mode_t scene_mode;
    camera_range_t et_range; /**< The exposure time range whose unit is us. */
} camera_ae_exposure_time_range_t;

/**
 * \struct camera_ae_gain_range_t: Provide supported gain range info per scene mode.
 */
typedef struct {
    camera_scene_mode_t scene_mode;
    camera_range_t gain_range; /**< The available sensor gain range whose unit is db. */
} camera_ae_gain_range_t;

/*
 * \enum camera_weight_grid_mode_t: Use to select which customized weight grid should be used.
 */
typedef enum {
    WEIGHT_GRID_AUTO,
    CUSTOM_WEIGHT_GRID_1,
    CUSTOM_WEIGHT_GRID_2,
    CUSTOM_WEIGHT_GRID_3,
    CUSTOM_WEIGHT_GRID_4,
    CUSTOM_WEIGHT_GRID_5,
    CUSTOM_WEIGHT_GRID_6,
    CUSTOM_WEIGHT_GRID_7,
    CUSTOM_WEIGHT_GRID_8,
    CUSTOM_WEIGHT_GRID_9,
    CUSTOM_WEIGHT_GRID_10,
    CUSTOM_WEIGHT_GRID_MAX
} camera_weight_grid_mode_t;

/**
 * \enum camera_yuv_color_range_mode_t: Specify which YUV color range will be used.
 */
typedef enum {
    CAMERA_FULL_MODE_YUV_COLOR_RANGE,   /*!< Full range (0 - 255) YUV data. */
    CAMERA_REDUCED_MODE_YUV_COLOR_RANGE /*!< Reduced range aka. BT.601 (16-235) YUV data range. */
} camera_yuv_color_range_mode_t;

/**
 * \enum camera_awb_mode_t: Used to control AWB working mode.
 */
typedef enum {
    AWB_MODE_AUTO,
    AWB_MODE_INCANDESCENT,
    AWB_MODE_FLUORESCENT,
    AWB_MODE_DAYLIGHT,
    AWB_MODE_FULL_OVERCAST,
    AWB_MODE_PARTLY_OVERCAST,
    AWB_MODE_SUNSET,
    AWB_MODE_VIDEO_CONFERENCE,
    AWB_MODE_MANUAL_CCT_RANGE,
    AWB_MODE_MANUAL_WHITE_POINT,
    AWB_MODE_MANUAL_GAIN,
    AWB_MODE_MANUAL_COLOR_TRANSFORM,
    AWB_MODE_MAX
} camera_awb_mode_t;

typedef enum { AWB_STATE_NOT_CONVERGED, AWB_STATE_CONVERGED } camera_awb_state_t;

/**
 * \enum camera_af_mode_t: Used to control af working mode.
 *
 * OFF:
 * Af algo is disabled, len position is controlled by application if supported.
 *
 * AUTO:
 * In this mode, the lens does not move unless the af trigger is activated.
 * The af algo will update af state every frame, and lock lens position when action is
 * completed.
 * The af trigger can be activated repeatedly.
 * Cancelling af trigger resets the lens position to default.
 *
 * MACRO:
 * Similar to AUTO and focus on objects very close to the camera.
 *
 * CONTINUOUS_VIDEO:
 * In this mode, the af algo modifies the lens position continually to
 * attempt to provide a constantly-in-focus image stream.
 * When the af trigger is activated,  af algo locks the lens position
 * until a cancel AF trigger is received.
 *
 * CONTINUOUS_PICTURE:
 * Similar to CONTINUOUS_VIDEO, except:
 * When the af trigger is activated, af algo can finish the current scan
 * before locking the lens position.
 *
 * Please refer to camera_af_trigger_t about how to trigger auto focus.
 * Please refer to camera_af_state_t about how to get autofocus result.
 */
typedef enum {
    AF_MODE_OFF,
    AF_MODE_AUTO,
    AF_MODE_MACRO,
    AF_MODE_CONTINUOUS_VIDEO,
    AF_MODE_CONTINUOUS_PICTURE,
    AF_MODE_MAX,
} camera_af_mode_t;

/**
 * \enum camera_af_trigger_t: Used trigger/cancel autofocus
 *
 * When af algo is enabled and it is changed to START, the HAL will
 * trigger autofocus.
 * When it is changed to CANCEL, the HAL will cancel any active trigger.
 *
 * Generally, applications should set it to START or CANCEL for only a
 * single frame capture, and then return it to IDLE, to get ready for
 * the next action.
 */
typedef enum {
    AF_TRIGGER_IDLE,
    AF_TRIGGER_START,
    AF_TRIGGER_CANCEL,
} camera_af_trigger_t;

/**
 * \enum camera_af_state_t: Used to return af state.
 */
typedef enum {
    AF_STATE_IDLE,            /*!< Focus is idle */
    AF_STATE_LOCAL_SEARCH,    /*!< Focus is in local search state */
    AF_STATE_EXTENDED_SEARCH, /*!< Focus is in extended search state */
    AF_STATE_SUCCESS,         /*!< Focus has succeeded */
    AF_STATE_FAIL             /*!< Focus has failed */
} camera_af_state_t;

/**
 * \enum camera_awb_mode_t: Used to control which preset effect will be used.
 */
typedef enum {
    CAM_EFFECT_NONE = 0,
    CAM_EFFECT_MONO,
    CAM_EFFECT_SEPIA,
    CAM_EFFECT_NEGATIVE,
    CAM_EFFECT_SKY_BLUE,
    CAM_EFFECT_GRASS_GREEN,
    CAM_EFFECT_SKIN_WHITEN_LOW,
    CAM_EFFECT_SKIN_WHITEN,
    CAM_EFFECT_SKIN_WHITEN_HIGH,
    CAM_EFFECT_VIVID,
} camera_effect_mode_t;

/**
 * \enum camera_test_pattern_mode_t: Use to control test pattern mode.
 */
typedef enum {
    TEST_PATTERN_OFF = 0,
    SOLID_COLOR,
    COLOR_BARS,
    COLOR_BARS_FADE_TO_GRAY,
    PN9,
    TEST_PATTERN_CUSTOM1,
} camera_test_pattern_mode_t;

/**
 * \enum camera_tonemap_mode_t: Use to control tonemap mode.
 */
typedef enum {
    TONEMAP_MODE_CONTRAST_CURVE,
    TONEMAP_MODE_FAST,
    TONEMAP_MODE_HIGH_QUALITY,
    TONEMAP_MODE_GAMMA_VALUE,
    TONEMAP_MODE_PRESET_CURVE,
} camera_tonemap_mode_t;

/**
 * \enum camera_tonemap_preset_curve_t: Use to control preset curve type.
 */
typedef enum {
    TONEMAP_PRESET_CURVE_SRGB,
    TONEMAP_PRESET_CURVE_REC709,
} camera_tonemap_preset_curve_t;

typedef struct {
    int32_t rSize;
    int32_t bSize;
    int32_t gSize;
    const float* rCurve;
    const float* bCurve;
    const float* gCurve;
} camera_tonemap_curves_t;

/**
 * \enum camera_msg_type_t: Use to indicate the type of message sent.
 */
typedef enum {
    CAMERA_EVENT_NONE = 0,
    CAMERA_ISP_BUF_READY,
    CAMERA_METADATA_READY,
    CAMERA_DEVICE_ERROR,
    CAMERA_IPC_ERROR,
    CAMERA_FRAME_DONE,
} camera_msg_type_t;

/**
 * \enum camera_power_mode_t: Use to control power mode.
 */
typedef enum {
    CAMERA_LOW_POWER = 0,
    CAMERA_HIGH_QUALITY,
} camera_power_mode_t;

/**
 * \enum raw_data_output_t: Use to control if output raw data
 */
typedef enum {
    CAMERA_RAW_DATA_OUTPUT_OFF = 0,
    CAMERA_RAW_DATA_OUTPUT_ON,
} raw_data_output_t;

/**
 * \struct Sensor data info for ZSL and YUV reprocessing.
 */
typedef struct {
    int64_t sequence;
    uint64_t timestamp;
} sensor_data_info_t;

/**
 * \struct isp_buffer_ready_t: Use to send isp buffer ready event data.
 */
typedef struct {
    uint32_t frameNumber;
    uint64_t timestamp;
} isp_buffer_ready_t;

/**
 * \struct metadata_ready_t: Use to send metadata ready event data.
 */
typedef struct {
    uint32_t frameNumber;
    int64_t sequence;
} metadata_ready_t;

typedef struct {
    int32_t streamId;
} frame_ready_t;

/**
 * \struct camera_msg_data_t: Use to specify msg data.
 */
typedef struct {
    camera_msg_type_t type;
    union {
        isp_buffer_ready_t buffer_ready;
        metadata_ready_t metadata_ready;
        frame_ready_t frame_ready;
    } data;
} camera_msg_data_t;

/**
 * \struct camera_callback_ops_t
 */
typedef struct camera_callback_ops {
    void (*notify)(const camera_callback_ops* cb, const camera_msg_data_t& data);
} camera_callback_ops_t;

/**
 * \struct camera_awb_gains_t: Used to specify AWB gain and AWB gain shift.
 */
typedef struct {
    int r_gain;
    int g_gain;
    int b_gain;
} camera_awb_gains_t;

/*!< camera_crop_region_t: Set crop region related parameters*/
typedef struct {
    int flag;
    int x;
    int y;
} camera_crop_region_t;

/**
 * \struct camera_color_transform_t: Specify the color transform matrix.
 */
typedef struct {
    float color_transform[3][3];
} camera_color_transform_t;

/**
 * \struct camera_color_gains_t: Specify the color correction gains.
 */
typedef struct {
    float color_gains_rggb[4];
} camera_color_gains_t;

/**
 * \enum camera_edge_mode_t: Specify the edge mode.
 */
typedef enum {
    EDGE_MODE_LEVEL_1, /* strength 20 */
    EDGE_MODE_LEVEL_2, /* strength 0 */
    EDGE_MODE_LEVEL_3, /* strength -60 */
    EDGE_MODE_LEVEL_4, /* strength -100 */
} camera_edge_mode_t;

/**
 * \enum camera_nr_mode_t: Specify the noise reduction mode.
 */
typedef enum {
    NR_MODE_LEVEL_1, /* strength 20 */
    NR_MODE_LEVEL_2, /* strength 0 */
    NR_MODE_LEVEL_3, /* strength -60 */
    NR_MODE_LEVEL_4, /* strength -100 */
} camera_nr_mode_t;

/**
 * \struct camera_nr_level_t: Specify the noise reduction level.
 */
typedef struct {
    int overall;
    int spatial;
    int temporal;
} camera_nr_level_t;

/**
 * \enum camera_iris_mode_t: Specify the IRIS mode.
 */
typedef enum {
    IRIS_MODE_AUTO,
    IRIS_MODE_MANUAL,
    IRIS_MODE_CUSTOMIZED,
} camera_iris_mode_t;

/**
 * \enum camera_wdr_mode_t: Specify the WDR/HDR mode. (deprecated)
 */
typedef enum {
    WDR_MODE_AUTO,
    WDR_MODE_ON,
    WDR_MODE_OFF,
} camera_wdr_mode_t;

/**
 * \enum camera_blc_area_mode_t: Switch black area mode.
 */
typedef enum {
    BLC_AREA_MODE_OFF,
    BLC_AREA_MODE_ON,
} camera_blc_area_mode_t;

/**
 * \struct camera_window_t: Used to specify AE/AWB weighted regions.
 */
typedef struct {
    int left;
    int top;
    int right;
    int bottom;
    int weight;
} camera_window_t;
typedef std::vector<camera_window_t> camera_window_list_t;

/**
 * \struct camera_image_enhancement_t: Used to specify the image enhancement effect.
 */
typedef struct {
    int sharpness;
    int brightness;
    int contrast;
    int hue;
    int saturation;
} camera_image_enhancement_t;

/**
 * \struct camera_coordinate_t: The coordinate of a point in a specified coordinate system.
 */
typedef struct {
    int x;
    int y;
} camera_coordinate_t;

/**
 * \struct camera_coordinate_system_t: Used to specify the coordinate system.
 */
typedef struct {
    int left;   /*!< Left coordinate value in the coordinate system. */
    int top;    /*!< Top coordinate value in the coordinate system. */
    int right;  /*!< Right coordinate value in the coordinate system. */
    int bottom; /*!< Bottom coordinate value in the coordinate system. */
} camera_coordinate_system_t;

/**
 * \struct camera_rational_t: Used to present a rational.
 */
typedef struct {
    int numerator;
    int denominator;
} camera_rational_t;

/**
 * \struct camera_awb_result_t: Present AWB result.
 */
typedef struct {
    float r_per_g; /*!< Accurate White Point (R) for the image: relative value*/
    float b_per_g; /*!< Accurate White Point (B) for the image. relative value*/
} camera_awb_result_t;

/**
 * \enum camera_converge_speed_t: Used to control AE/AWB converge speed.
 */
typedef enum { CONVERGE_NORMAL, CONVERGE_MID, CONVERGE_LOW, CONVERGE_MAX } camera_converge_speed_t;

/**
 * \enum camera_converge_speed_mode_t: Used to control AE/AWB converge speed mode.
 */
typedef enum {
    CONVERGE_SPEED_MODE_AIQ, /*!< Use AIQ algo to control converge speed. */
    CONVERGE_SPEED_MODE_HAL  /*!< Implement converge speed control in HAL. */
} camera_converge_speed_mode_t;

/**
 * \enum camera_ae_distribution_priority_t: Used to control exposure priority mode.
 */
typedef enum {
    DISTRIBUTION_AUTO,    /*!< The AIQ algo decides completely */
    DISTRIBUTION_SHUTTER, /*!< Shutter speed priority mode */
    DISTRIBUTION_ISO,     /*!< ISO priority mode */
    DISTRIBUTION_APERTURE /*!< Aperture priority mode */
} camera_ae_distribution_priority_t;

/**
 * \enum camera_deinterlace_mode_t: Used to control the deinterlace mode.
 */
typedef enum {
    DEINTERLACE_OFF,    /*!< Do not do any deinterlace */
    DEINTERLACE_WEAVING /*!< Weave the two frame buffers into one. */
} camera_deinterlace_mode_t;

/**
 * \enum camera_fisheye_dewarping_mode_t: Used to control the dewarping mode.
 */
typedef enum {
    FISHEYE_DEWARPING_OFF,
    FISHEYE_DEWARPING_REARVIEW,
    FISHEYE_DEWARPING_HITCHVIEW
} camera_fisheye_dewarping_mode_t;

/**
 * \enum camera_makernote_mode_t: Used to control makernote mode.
 */
typedef enum {
    MAKERNOTE_MODE_OFF,
    MAKERNOTE_MODE_JPEG,
    MAKERNOTE_MODE_RAW
} camera_makernote_mode_t;

/**
 * \enum camera_ldc_mode_t: Used to toggle lens distortion correction.
 */
typedef enum { LDC_MODE_OFF, LDC_MODE_ON } camera_ldc_mode_t;

/**
 * \enum camera_rsc_mode_t: Used to toggle rolling shutter correction.
 */
typedef enum { RSC_MODE_OFF, RSC_MODE_ON } camera_rsc_mode_t;

/**
 * \enum camera_flip_mode_t: Used to set output slip.
 */
typedef enum {
    FLIP_MODE_NONE = 0,
    FLIP_MODE_VFLIP,
    FLIP_MODE_HFLIP,
    FLIP_MODE_VHFLIP
} camera_flip_mode_t;

/**
 * \enum camera_mono_downscale_mode_t: Used to enable/disable MONO Downscale.
 */
typedef enum { MONO_DS_MODE_OFF, MONO_DS_MODE_ON } camera_mono_downscale_mode_t;

/**
 * \enum camera_video_stabilization_mode_t: Used to control the video stabilization mode.
 */
typedef enum {
    VIDEO_STABILIZATION_MODE_OFF,
    VIDEO_STABILIZATION_MODE_ON
} camera_video_stabilization_mode_t;
typedef std::vector<camera_video_stabilization_mode_t> camera_video_stabilization_list_t;

/**
 * \enum camera_mount_type_t: camera mount type
 */
typedef enum {
    WALL_MOUNTED,
    CEILING_MOUNTED,
} camera_mount_type_t;

/**
 * \enum camera_shading_mode_t: camera shading mode type
 */
typedef enum {
    SHADING_MODE_OFF,
    SHADING_MODE_FAST,
    SHADING_MODE_HIGH_QUALITY
} camera_shading_mode_t;

/**
 * \enum camera_lens_shading_map_mode_type_t: camera lens shading map mode type
 */
typedef enum {
    LENS_SHADING_MAP_MODE_OFF,
    LENS_SHADING_MAP_MODE_ON
} camera_lens_shading_map_mode_type_t;

typedef enum {
    CAMERA_STATISTICS_FACE_DETECT_MODE_OFF,
    CAMERA_STATISTICS_FACE_DETECT_MODE_SIMPLE,
    CAMERA_STATISTICS_FACE_DETECT_MODE_FULL,
} camera_statistics_face_detect_mode_t;

typedef enum {
    ROTATE_NONE,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
    ROTATE_AUTO,
} camera_rotate_mode_t;

/**
 * \struct camera_zoom_region_t: Used to specify zoom regions.
 */
typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    float ratio;
    camera_rotate_mode_t rotateMode;
} camera_zoom_region_t;

#define IS_INPUT_BUFFER(timestamp, sequence) ((timestamp) > 0 && (sequence) >= 0)
}  // namespace icamera
