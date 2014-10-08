#ifndef RASPBERRY_PI_CAMERA
#define RASPBERRY_PI_CAMERA

#include <iostream>

#include <cstdint>
#include <stdexcept>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>

extern "C"
{
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "raspicam/RaspiCamControl.h"
#include "raspicam/RaspiPreview.h"

int mmal_status_to_int(MMAL_STATUS_T status); // from RaspiCamControl.c

} // "C"


// Standard port setting for the camera component
enum
{
    MMAL_CAMERA_PREVIEW_PORT = 0,
    MMAL_CAMERA_VIDEO_PORT = 1,
    MMAL_CAMERA_CAPTURE_PORT = 2
};

// forward
static void camera_control_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer);
static void encoder_buffer_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer);


namespace rpi_cam
{
    class MmalException : public std::exception
    {
    public:
        MMAL_STATUS_T status;
        const char * msg;

        MmalException(MMAL_STATUS_T s, const char * message)
        :   status(s),
            msg(message)
        {}

        virtual const char * what() const noexcept { return msg; }
    };

    /// Structure containing all state information for the current run
    struct RaspividState
    {
        static const unsigned DEFAULT_BITRATE = 17000000;
        static const unsigned FHD_WIDTH = 1920;
        static const unsigned FHD_HEIGHT = 1080;

        // Video format information (0 implies variable)
        static const unsigned VIDEO_FRAME_RATE_NUM = 30;
        static const unsigned VIDEO_FRAME_RATE_DEN = 1;

        int timeout;                    ///< Time taken before frame is grabbed and app then shuts down. Units are milliseconds
        int width;                      ///< Requested width of image
        int height;                     ///< requested height of image

        int framerate;                  ///< Requested frame rate (fps)
        int bitrate;                    ///< Requested bitrate
        unsigned quantisationParameter; ///< Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate

        int cameraNum;                  ///< Camera number
        int settings;                   ///< Request settings from the camera
        int sensor_mode;                ///< Sensor mode. 0=auto. Check docs/forum for modes selected by other values.

        RaspividState()
        :   timeout(5000),
            width(FHD_WIDTH),
            height(FHD_HEIGHT),
            framerate(VIDEO_FRAME_RATE_NUM),
            bitrate(DEFAULT_BITRATE),
            quantisationParameter(0),
            cameraNum(0),
            settings(0),
            sensor_mode(0)
        {}

        void setDynamicFramerate(int shutter_speed)
        {
            if (shutter_speed)
            {
                if (framerate > 1000000./shutter_speed)
                    framerate = 0;
            }
        }

        void streamFormat(MMAL_ES_FORMAT_T * format) const
        {
            format->encoding = MMAL_ENCODING_OPAQUE;
            format->encoding_variant = MMAL_ENCODING_I420;

            format->es->video.width = VCOS_ALIGN_UP(width, 32);
            format->es->video.height = VCOS_ALIGN_UP(height, 16);
            format->es->video.crop.x = 0;
            format->es->video.crop.y = 0;
            format->es->video.crop.width = width;
            format->es->video.crop.height = height;
            format->es->video.frame_rate.num = framerate;
            format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
        }

        // timeout seconds at bitrate Mbits
        unsigned bufferSize() const
        {
            if (bitrate == 0)
                throw MmalException(MMAL_SUCCESS, "error circular buffer requires constant bitrate and small intra period");
            if (timeout == 0)
                throw MmalException(MMAL_SUCCESS, "error circular buffer size is based on timeout must be greater than zero");

            return bitrate * (timeout / 1000) / 8;
        }
    };

    ///
    class Frame
    {
    public:
        int64_t  pts;   ///< Presentation timestamp in microseconds.
        int64_t  dts;   ///< Decode timestamp in microseconds (dts = pts, except in the case of video streams with B frames)

        Frame(std::vector<uint8_t>& data, uint32_t flags)
        :   pts(0),
            dts(0),
            flags_(flags)
        {
            data_.swap(data);
        }

        bool eos() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_EOS; }
        bool isKey() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_KEYFRAME; }
        bool discont() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY; }
        bool config() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_CONFIG; }
        bool encrypt() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED; }
        bool codecInfo() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO; }
        bool snapshot() const { return flags_ & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT; }
        bool corrupted() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_CORRUPTED; }
        bool transmissed() const { return ! (flags_ & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED); }
        bool decodeOnly() const { return flags_ & MMAL_BUFFER_HEADER_FLAG_DECODEONLY; }

        void swap(std::vector<uint8_t>& out) { out.swap(data_); }

    private:
        std::vector<uint8_t> data_;
        uint32_t flags_;
    };

    typedef std::shared_ptr<Frame> FramePtr;

    /// Struct used to pass information in encoder port userdata to callback
    struct EncCallbackData
    {
        MMAL_POOL_T * encoderPool;
        std::vector<uint8_t> curFrame;

        std::mutex queueMutex;
        std::deque<FramePtr> queue;

        FramePtr nextFrame()
        {
            std::lock_guard<std::mutex> lock( queueMutex ); // LOCK

            FramePtr frame;
            if (! queue.empty())
            {
                frame = *queue.begin();
                queue.pop_front();
            }

            return frame;
        }
    };

    ///
    class CameraComponent
    {
    public:
        /// Video render needs at least 2 buffers.
        static const unsigned VIDEO_OUTPUT_BUFFERS_NUM = 3;

        CameraComponent(RaspividState& state, int shutter_speed)
        :   camera_(nullptr),
            previewPort_(nullptr),
            videoPort_(nullptr),
            stillPort_(nullptr)
        {
            resetCamConfig(state.width, state.height);

            MMAL_STATUS_T status;

            status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Failed to create camera component");

            MMAL_PARAMETER_INT32_T camera_num = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state.cameraNum};

            status = mmal_port_parameter_set(camera_->control, &camera_num.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Could not select camera");

            if (!camera_->output_num)
            {
                status = MMAL_ENOSYS;
                throw MmalException(status, "Camera doesn't have output ports");
            }

            status = mmal_port_parameter_set_uint32(camera_->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state.sensor_mode);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Could not set sensor mode");

            previewPort_ = camera_->output[MMAL_CAMERA_PREVIEW_PORT];
            videoPort_ = camera_->output[MMAL_CAMERA_VIDEO_PORT];
            stillPort_ = camera_->output[MMAL_CAMERA_CAPTURE_PORT];

            if (state.settings)
            {
                MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
                    {{MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)}, MMAL_PARAMETER_CAMERA_SETTINGS, 1};

                status = mmal_port_parameter_set(camera_->control, &change_event_request.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "No camera settings events");
            }

            // Enable the camera, and tell it its control callback function
            status = mmal_port_enable(camera_->control, camera_control_callback);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to enable control port");

            // setup the camera configuration
            mmal_port_parameter_set(camera_->control, &camConfig_.hdr);


            // Now set up the port formats

            // Set the encode format on the Preview port
            // HW limitations mean we need the preview to be the same size as the required recorded output

            state.streamFormat(previewPort_->format);
            previewPort_->format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
            previewPort_->format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

            if (shutter_speed > 6000000)
            {
                MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, { 50, 1000 }, {166, 1000}};
                mmal_port_parameter_set(previewPort_, &fps_range.hdr);
            }
            else if (shutter_speed > 1000000)
            {
                MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, { 166, 1000 }, {999, 1000}};
                mmal_port_parameter_set(previewPort_, &fps_range.hdr);
            }

            // enable dynamic framerate if necessary
            state.setDynamicFramerate(shutter_speed);

            status = mmal_port_format_commit(previewPort_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "camera viewfinder format couldn't be set");


            // Set the encode format on the video port

            state.streamFormat(videoPort_->format);

            if (shutter_speed > 6000000)
            {
                MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, { 50, 1000 }, {166, 1000}};
                mmal_port_parameter_set(videoPort_, &fps_range.hdr);
            }
            else if (shutter_speed > 1000000)
            {
                MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, { 167, 1000 }, {999, 1000}};
                mmal_port_parameter_set(videoPort_, &fps_range.hdr);
            }

            status = mmal_port_format_commit(videoPort_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "camera video format couldn't be set");

            // Ensure there are enough buffers to avoid dropping frames
            if (videoPort_->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
                videoPort_->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


            // Set the encode format on the still port
            state.streamFormat(stillPort_->format);
            stillPort_->format->es->video.frame_rate.num = 0;
            stillPort_->format->es->video.frame_rate.den = 1;

            status = mmal_port_format_commit(stillPort_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "camera still format couldn't be set");

            // Ensure there are enough buffers to avoid dropping frames
            if (stillPort_->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
                stillPort_->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

            // Enable component
            status = mmal_component_enable(camera_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "camera component couldn't be enabled");
        }

        ~CameraComponent()
        {
            if (camera_)
            {
                if (stillPort_ && stillPort_->is_enabled)
                    mmal_port_disable(stillPort_);

                mmal_component_disable(camera_);
                mmal_component_destroy(camera_);
            }
        }

        MMAL_COMPONENT_T * component() { return camera_; }
        MMAL_PORT_T * previewPort() { return previewPort_; }
        MMAL_PORT_T * videoPort() { return videoPort_; }


        const MMAL_PARAMETER_CAMERA_CONFIG_T& config() const { return camConfig_; }

    private:
        MMAL_COMPONENT_T * camera_;
        MMAL_PORT_T * previewPort_;
        MMAL_PORT_T * videoPort_;
        MMAL_PORT_T * stillPort_;
        MMAL_PARAMETER_CAMERA_CONFIG_T camConfig_;

        void resetCamConfig(unsigned width, unsigned height)
        {
            camConfig_.hdr = { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(camConfig_) };
            camConfig_.max_stills_w = width;
            camConfig_.max_stills_h = height;
            camConfig_.stills_yuv422 = 0;
            camConfig_.one_shot_stills = 0;
            camConfig_.max_preview_video_w = width;
            camConfig_.max_preview_video_h = height;
            camConfig_.num_preview_video_frames = 3;
            camConfig_.stills_capture_circular_buffer_height = 0;
            camConfig_.fast_preview_resume = 0;
            camConfig_.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;
        }
    };

    ///
    class EncoderComponent
    {
    public:
        EncoderComponent(const MMAL_PARAMETER_VIDEO_PROFILE_T& profile, unsigned bitrate, unsigned quantisationParameter)
        :   encoder_(nullptr),
            pool_(nullptr)
        {
            MMAL_STATUS_T status;

            status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to create video encoder component");

            if (!encoder_->input_num || !encoder_->output_num)
                throw MmalException(MMAL_ENOSYS, "Video encoder doesn't have input/output ports");

            MMAL_PORT_T * encoder_input = encoder_->input[0];
            MMAL_PORT_T * encoder_output = encoder_->output[0];

            // We want same format on input and output
            mmal_format_copy(encoder_output->format, encoder_input->format);

            // Only supporting H264 at the moment
            encoder_output->format->encoding = MMAL_ENCODING_H264;
            encoder_output->format->bitrate = bitrate;

            encoder_output->buffer_size = encoder_output->buffer_size_recommended;
            if (encoder_output->buffer_size < encoder_output->buffer_size_min)
                encoder_output->buffer_size = encoder_output->buffer_size_min;

            encoder_output->buffer_num = encoder_output->buffer_num_recommended;
            if (encoder_output->buffer_num < encoder_output->buffer_num_min)
                encoder_output->buffer_num = encoder_output->buffer_num_min;

            // We need to set the frame rate on output to 0, to ensure it gets
            // updated correctly from the input framerate when port connected
            encoder_output->format->es->video.frame_rate.num = 0;
            encoder_output->format->es->video.frame_rate.den = 1;

            // Commit the port changes to the output port
            status = mmal_port_format_commit(encoder_output);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set format on video encoder output port");

            // Set the rate control parameter
#if 0
            {
                MMAL_PARAMETER_VIDEO_RATECONTROL_T param = {{ MMAL_PARAMETER_RATECONTROL, sizeof(param)}, MMAL_VIDEO_RATECONTROL_DEFAULT};
                status = mmal_port_parameter_set(encoder_output, &param.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set ratecontrol");
            }
#endif
#if 0
            if (intraperiod != -1)
            {
                MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, (unsigned)intraperiod};
                status = mmal_port_parameter_set(encoder_output, &param.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set intraperiod");
            }
#endif
            if (quantisationParameter)
            {
                MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, quantisationParameter};
                status = mmal_port_parameter_set(encoder_output, &param.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set initial QP");

                MMAL_PARAMETER_UINT32_T param2 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)}, quantisationParameter};
                status = mmal_port_parameter_set(encoder_output, &param2.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set min QP");

                MMAL_PARAMETER_UINT32_T param3 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)}, quantisationParameter};
                status = mmal_port_parameter_set(encoder_output, &param3.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set max QP");
            }

            status = mmal_port_parameter_set(encoder_output, &profile.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set H264 profile");
#if 0
            status = mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, MMAL_TRUE);
            if (status != MMAL_SUCCESS)
            {
                vcos_log_error("Unable to set immutable input flag");
                // Continue rather than abort..
            }
#endif
            //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
            status = mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, MMAL_TRUE);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set INLINE HEADER FLAG parameters");

            //  Enable component
            status = mmal_component_enable(encoder_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to enable video encoder component");

            /* Create pool of buffer headers for the output port to consume */
            pool_ = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);
            if (!pool_)
            {
                vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
            }
        }

        ~EncoderComponent()
        {
            MMAL_PORT_T * port = outputPort();
            if (port && port->is_enabled)
                mmal_port_disable(port);

            if (encoder_)
                mmal_component_disable(encoder_);

            if (pool_)
                mmal_port_pool_destroy(encoder_->output[0], pool_);

            if (encoder_)
                mmal_component_destroy(encoder_);
        }

        MMAL_POOL_T * pool() { return pool_; }

        MMAL_PORT_T * inputPort() { return encoder_->input[0]; }
        MMAL_PORT_T * outputPort() { return encoder_->output[0]; }

    private:
        MMAL_COMPONENT_T * encoder_;
        MMAL_POOL_T * pool_;
    };

    ///
    class Connection
    {
    public:
        Connection()
        :   connection_(nullptr)
        {}

        Connection(MMAL_PORT_T * outputPort, MMAL_PORT_T * inputPort)
        {
            connect(outputPort, inputPort);
        }

        ~Connection()
        {
            if (connection_)
                mmal_connection_destroy(connection_);
        }

        void connect(MMAL_PORT_T * outputPort, MMAL_PORT_T * inputPort)
        {
            MMAL_STATUS_T status = mmal_connection_create(&connection_, outputPort, inputPort,
                                       MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
            if (status == MMAL_SUCCESS)
                status = mmal_connection_enable(connection_);

            /// @warning check both fuctions failure
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Failed to connect ports");
        }

    private:
        MMAL_CONNECTION_T * connection_;
    };

    ///
    class RasPiPreview
    {
    public:
        RasPiPreview(bool wantPreview = true)
        {
            MMAL_STATUS_T status;

            setDefaults();

            preview_.wantPreview = wantPreview;
            preview_.preview_component = nullptr;

            if (!wantPreview)
            {
                // No preview required, so create a null sink component to take its place
                status = mmal_component_create("vc.null_sink", &preview_.preview_component);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to create null sink component");
            }
            else
            {
                status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &preview_.preview_component);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to create preview component");

                if (!preview_.preview_component->input_num)
                    throw MmalException(MMAL_SUCCESS, "No input ports found on component");

                MMAL_PORT_T * preview_port = preview_.preview_component->input[0];

                MMAL_DISPLAYREGION_T param;
                param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
                param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

                param.set = MMAL_DISPLAY_SET_LAYER;
                param.layer = PREVIEW_LAYER;

                param.set |= MMAL_DISPLAY_SET_ALPHA;
                param.alpha = preview_.opacity;

                if (preview_.wantFullScreenPreview)
                {
                    param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
                    param.fullscreen = 1;
                }
                else
                {
                    param.set |= (MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_FULLSCREEN);
                    param.fullscreen = 0;
                    param.dest_rect = preview_.previewWindow;
                }

                status = mmal_port_parameter_set(preview_port, &param.hdr);
                if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
                    throw MmalException(status, "Unable to set preview port parameters");
            }

            status = mmal_component_enable(preview_.preview_component);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to enable preview/null sink component");
        }

        ~RasPiPreview()
        {
            if (preview_.preview_component)
            {
                mmal_component_disable(preview_.preview_component);
                mmal_component_destroy(preview_.preview_component);
                preview_.preview_component = nullptr;
            }
        }

        MMAL_PORT_T * inputPort() { return preview_.preview_component->input[0]; }
        RASPIPREVIEW_PARAMETERS * params() { return &preview_; }

        void setDefaults()
        {
            preview_.wantPreview = 1;
            preview_.wantFullScreenPreview = 1;
            preview_.opacity = 255;
            preview_.previewWindow.x = 0;
            preview_.previewWindow.y = 0;
            preview_.previewWindow.width = 1024;
            preview_.previewWindow.height = 768;
            preview_.preview_component = nullptr;
        }

    private:
        RASPIPREVIEW_PARAMETERS preview_;
    };

    ///
    class RaspberryPiCamera
    {
    public:
        RaspberryPiCamera(bool wantPreview = true)
        :   isOK_(false)
        {
            //bcm_host_init();
            vcos_log_register("RPiCam", VCOS_LOG_CATEGORY);

            try
            {
                {
                    RASPICAM_CAMERA_PARAMETERS cameraParameters;
                    raspicamcontrol_set_defaults(&cameraParameters);

                    camera_ = std::make_shared<CameraComponent>(state_, cameraParameters.shutter_speed);

                    raspicamcontrol_set_all_parameters(camera_->component(), &cameraParameters);
                }

                {
                    MMAL_PARAMETER_VIDEO_PROFILE_T profile;
                    profile.hdr.id = MMAL_PARAMETER_PROFILE;
                    profile.hdr.size = sizeof(profile);
                    profile.profile[0].profile = MMAL_VIDEO_PROFILE_H264_HIGH;
                    profile.profile[0].level = MMAL_VIDEO_LEVEL_H264_4; // This is the only value supported

                    encoder_ = std::make_shared<EncoderComponent>(profile, state_.bitrate, state_.quantisationParameter);
                    callbackData_.encoderPool = encoder_->pool();
                }

                {
                    preview_ = std::make_shared<RasPiPreview>(wantPreview);
                    if (wantPreview)
                        connPreview_ = std::make_shared<Connection>(camera_->previewPort(), preview_->inputPort());
                }

                {
                    connEncoder_ = std::make_shared<Connection>(camera_->videoPort(), encoder_->inputPort());

                    encoder_->outputPort()->userdata = (struct MMAL_PORT_USERDATA_T *) &callbackData_;

                    MMAL_STATUS_T status = mmal_port_enable(encoder_->outputPort(), encoder_buffer_callback);
                    if (status != MMAL_SUCCESS)
                        throw MmalException(status, "Failed to setup encoder output");
                }

                // Send all the buffers to the encoder output port
                {
                    unsigned qLen = mmal_queue_length(callbackData_.encoderPool->queue);
                    for (unsigned i = 0; i < qLen; i++)
                    {
                        MMAL_BUFFER_HEADER_T * buffer = mmal_queue_get(callbackData_.encoderPool->queue);
                        if (!buffer)
                            vcos_log_error("Unable to get a required buffer %d from pool queue", i);

                        MMAL_STATUS_T status = mmal_port_send_buffer(encoder_->outputPort(), buffer);
                        if (status != MMAL_SUCCESS)
                            vcos_log_error("Unable to send a buffer to encoder output port (%d)", i);
                    }
                }

                isOK_ = true;
            }
            catch (const MmalException& e)
            {
                mmal_status_to_int(e.status);
                vcos_log_error(e.msg);
                raspicamcontrol_check_configuration(128);
            }
        }

        ~RaspberryPiCamera()
        {
            // destroy connections before components
            connPreview_.reset();
            connEncoder_.reset();
        }

        bool isOK() const { return isOK_; }
        bool startCapture() { return mmal_port_parameter_set_boolean(camera_->videoPort(), MMAL_PARAMETER_CAPTURE, MMAL_TRUE) == MMAL_SUCCESS; }
        bool stopCapture() { return mmal_port_parameter_set_boolean(camera_->videoPort(), MMAL_PARAMETER_CAPTURE, MMAL_FALSE) == MMAL_SUCCESS; }

        bool nextFrame(std::vector<uint8_t>& outData, int64_t& pts, bool& isKey)
        {
            FramePtr frame = callbackData_.nextFrame();
            if (!frame)
                return false;

            frame->swap(outData);
            pts = frame->pts;
            isKey = frame->isKey();
            return true;
        }

    private:
        RaspividState state_;
        EncCallbackData callbackData_;
        std::shared_ptr<CameraComponent> camera_;
        std::shared_ptr<EncoderComponent> encoder_;
        std::shared_ptr<RasPiPreview> preview_;
        std::shared_ptr<Connection> connPreview_;
        std::shared_ptr<Connection> connEncoder_;
        bool isOK_;
    };
}

/// buffer header callback function for camera control
///
/// @param port Pointer to port from which callback originated
/// @param buffer mmal buffer header pointer
///
static void camera_control_callback(MMAL_PORT_T * /*port*/, MMAL_BUFFER_HEADER_T * buffer)
{
    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
    {
        MMAL_EVENT_PARAMETER_CHANGED_T * param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
        switch (param->hdr.id)
        {
            case MMAL_PARAMETER_CAMERA_SETTINGS:
            {
                MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
                vcos_log_error("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                               settings->exposure,
                               settings->analog_gain.num, settings->analog_gain.den,
                               settings->digital_gain.num, settings->digital_gain.den);
                vcos_log_error("AWB R=%u/%u, B=%u/%u",
                               settings->awb_red_gain.num, settings->awb_red_gain.den,
                               settings->awb_blue_gain.num, settings->awb_blue_gain.den
                               );
                break;
            }
        }
    }
    else
    {
        vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}


/// buffer header callback function for encoder
///
/// @param port Pointer to port from which callback originated
/// @param buffer mmal buffer header pointer
///
static void encoder_buffer_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer)
{
    static const unsigned MAX_QUEUE_SIZE = 32;

    rpi_cam::EncCallbackData * pData = (rpi_cam::EncCallbackData *)port->userdata;

    if (buffer->cmd)
        vcos_log_error("encoder callback: received unexpected callback event, 0x%08x", buffer->cmd);

    if (pData)
    {
#if 0
        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED)
            vcos_log_error("encoder callback: MMAL_BUFFER_HEADER_FLAG_CORRUPTED");

        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED)
            vcos_log_error("encoder callback: MMAL_BUFFER_HEADER_FLAG_ENCRYPTED");

        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)
            vcos_log_error("encoder callback: MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED");
#endif
        //if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START)
        //    pData->curFrame.clear();

        mmal_buffer_header_mem_lock(buffer); // BUFFER LOCK

        pData->curFrame.insert(pData->curFrame.end(), buffer->data, buffer->data + buffer->length);

        mmal_buffer_header_mem_unlock(buffer); // BUFFER UNLOCK

        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END
            // HACK: mediaserver don't understand headers without frame - merge them
            && !(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG))
        {
            std::lock_guard<std::mutex>( pData->queueMutex ); // LOCK

            while (pData->queue.size() >= MAX_QUEUE_SIZE)
                pData->queue.pop_front();

            pData->queue.push_back( std::make_shared<rpi_cam::Frame>(pData->curFrame, buffer->flags) );
            pData->curFrame.clear();

            if (buffer->pts != MMAL_TIME_UNKNOWN)
                pData->queue.back()->pts = buffer->pts;

            if (buffer->dts != MMAL_TIME_UNKNOWN)
                pData->queue.back()->dts = buffer->dts;

            // TODO: pts correction
            //int64_t base_time = vcos_getmicrosecs64()/1000;
        }
    }
    else
    {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (pData && pData->encoderPool && port->is_enabled)
    {
        MMAL_STATUS_T status;
        MMAL_BUFFER_HEADER_T * new_buffer = mmal_queue_get(pData->encoderPool->queue);
        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the encoder port");
    }
}

#endif // RASPBERRY_PI_CAMERA
