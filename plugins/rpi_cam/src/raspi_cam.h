#ifndef RASPBERRY_PI_CAMERA
#define RASPBERRY_PI_CAMERA

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
static void h264_encoder_buffer_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer);
//static void jpeg_encoder_buffer_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer);


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

    ///
    /// Resolution	Aspect Ratio    Framerates	Video	Image	FoV
    /// 2592x1944   4:3             1-15fps     x       x       Full
    /// 1296x972    4:3             1-42fps     x               Full
    /// 1296x730    16:9            1-49fps     x               Full
    /// 640x480     4:3             42.1-60fps  x               Full
    /// 640x480     4:3             60.1-90fps  x               Full
    /// 1920x1080   16:9            1-30fps     x               Partial
    ///
    struct VideoMode
    {
        enum Mode
        {
            VM_1080p30,
            VM_720p50,
            VM_640x480p60,
            VM_640x480p90,
            //
            JM_480x270,
            JM_320x240
        };

        unsigned width;         ///< Requested width of image
        unsigned height;        ///< Requested height of image
        unsigned framerate;     ///< Requested frame rate (fps)

        VideoMode(Mode mode, bool dynFramerate = false)
        {
            switch (mode)
            {
                case VM_1080p30:
                    width = 1920;
                    height = 1080;
                    framerate = 30;
                    break;

                case VM_720p50:
                    width = 1296;
                    height = 730;
                    framerate = 49;
                    break;

                case VM_640x480p60:
                    width = 640;
                    height = 480;
                    framerate = 60;
                    break;

                case VM_640x480p90:
                    width = 640;
                    height = 480;
                    framerate = 90;
                    break;

                case JM_480x270:
                    width = 480;
                    height = 270;
                    framerate = 0;
                    break;

                case JM_320x240:
                    width = 320;
                    height = 240;
                    framerate = 0;
                    break;
            }

            if (dynFramerate)
                framerate = 0;
        }
    };

    ///
    struct Bitrate
    {
        static const unsigned DEFAULT_BITRATE = 8000000;

        unsigned bitrate;       ///< Requested bitrate
        unsigned quantisation;  ///< Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate

        Bitrate(unsigned br = DEFAULT_BITRATE, unsigned quant = 0)
        :   bitrate(br),
            quantisation(quant)
        {}
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

        CameraComponent(int cameraNum, const VideoMode& mode, unsigned sensorMode = 0, bool requestSettings = false)
        :   camera_(nullptr),
            previewPort_(nullptr),
            videoPort_(nullptr),
            stillPort_(nullptr)
        {
            resetCamConfig(mode.width, mode.height);

            MMAL_STATUS_T status;

            status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Failed to create camera component");

            MMAL_PARAMETER_INT32_T camNo = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camNo)}, cameraNum};
            status = mmal_port_parameter_set(camera_->control, &camNo.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Could not select camera");

            if (!camera_->output_num)
                throw MmalException(MMAL_ENOSYS, "Camera doesn't have output ports");

            status = mmal_port_parameter_set_uint32(camera_->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, sensorMode);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Could not set sensor mode");

            if (requestSettings)
            {
                MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T changeEventRequest =
                    {{MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)}, MMAL_PARAMETER_CAMERA_SETTINGS, 1};

                status = mmal_port_parameter_set(camera_->control, &changeEventRequest.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "No camera settings events");
            }

            // Enable the camera, and tell it its control callback function
            status = mmal_port_enable(camera_->control, camera_control_callback);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Could not enable camera control port");

            // setup the camera configuration
            status = mmal_port_parameter_set(camera_->control, &camConfig_.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Could not set camera config");

            previewPort_ = camera_->output[MMAL_CAMERA_PREVIEW_PORT];
            videoPort_ = camera_->output[MMAL_CAMERA_VIDEO_PORT];
            stillPort_ = camera_->output[MMAL_CAMERA_CAPTURE_PORT];

            // Now set up the port formats

            // Set the encode format on the Preview port
            // HW limitations mean we need the preview to be the same size as the required recorded output

            streamFormat(mode, previewPort_->format);
            //previewPort_->format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
            //previewPort_->format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

            status = mmal_port_format_commit(previewPort_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "camera viewfinder format couldn't be set");


            // Set the encode format on the video port

            streamFormat(mode, videoPort_->format);
            status = mmal_port_format_commit(videoPort_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "camera video format couldn't be set");

            // Ensure there are enough buffers to avoid dropping frames
            if (videoPort_->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
                videoPort_->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

            // Set the encode format on the still port

            streamFormat(mode, stillPort_->format);
            //stillPort_->format->encoding = MMAL_ENCODING_OPAQUE;
            stillPort_->format->es->video.frame_rate.num = 0;

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

            // Camera parameters

            RASPICAM_CAMERA_PARAMETERS parameters;
            raspicamcontrol_set_defaults(&parameters);

            parameters.exposureMode = MMAL_PARAM_EXPOSUREMODE_FIXEDFPS;
            parameters.awbMode = MMAL_PARAM_AWBMODE_AUTO;

            raspicamcontrol_set_all_parameters(camera_, &parameters);
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
        //MMAL_PORT_T * stillPort() { return stillPort_; }


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

        void streamFormat(const VideoMode& mode, MMAL_ES_FORMAT_T * format) const
        {
            static const unsigned VIDEO_FRAME_RATE_DEN = 1;

            format->encoding = MMAL_ENCODING_OPAQUE;
            //format->encoding = MMAL_ENCODING_I420; // not MMAL_ENCODING_OPAQUE for splitter !!!
            format->encoding_variant = MMAL_ENCODING_I420;

            format->es->video.width = VCOS_ALIGN_UP(mode.width, 32);
            format->es->video.height = VCOS_ALIGN_UP(mode.height, 16);
            format->es->video.crop.x = 0;
            format->es->video.crop.y = 0;
            format->es->video.crop.width = mode.width;
            format->es->video.crop.height = mode.height;
            format->es->video.frame_rate.num = mode.framerate;
            format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
        }
    };

    ///
    class EncoderComponent
    {
    public:
        MMAL_PORT_T * inputPort() { return encoder_->input[0]; }
        MMAL_PORT_T * outputPort() { return encoder_->output[0]; }

        MMAL_POOL_T * pool() { return pool_; }

    protected:
        MMAL_COMPONENT_T * encoder_;
        MMAL_POOL_T * pool_;

        EncoderComponent()
        :   encoder_(nullptr),
            pool_(nullptr)
        {}

        virtual void setFormat() = 0;
        virtual void setParameters() = 0;

        void init(const char * component)
        {
            MMAL_STATUS_T status;

            status = mmal_component_create(component, &encoder_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to create encoder component");

            if (!encoder_->input_num || !encoder_->output_num)
                throw MmalException(MMAL_ENOSYS, "Encoder doesn't have input/output ports");

            // We want same format on input and output
            mmal_format_copy(outputPort()->format, inputPort()->format);

            setFormat(); //

            outputPort()->buffer_size = outputPort()->buffer_size_recommended;
            if (outputPort()->buffer_size < outputPort()->buffer_size_min)
                outputPort()->buffer_size = outputPort()->buffer_size_min;

            outputPort()->buffer_num = outputPort()->buffer_num_recommended;
            if (outputPort()->buffer_num < outputPort()->buffer_num_min)
                outputPort()->buffer_num = outputPort()->buffer_num_min;

            // We need to set the frame rate on output to 0, to ensure it gets
            // updated correctly from the input framerate when port connected
            outputPort()->format->es->video.frame_rate.num = 0;
            outputPort()->format->es->video.frame_rate.den = 1;

            // Commit the port changes to the output port
            status = mmal_port_format_commit(outputPort());
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set format on encoder output port");

            setParameters(); //

            //  Enable component
            status = mmal_component_enable(encoder_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to enable encoder component");

            // Create pool of buffer headers for the output port to consume
            pool_ = mmal_port_pool_create(outputPort(), outputPort()->buffer_num, outputPort()->buffer_size);
            if (!pool_)
                throw MmalException(status, "Failed to create buffer header pool for encoder output port");
        }

        // not virtual
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
    };

    ///
    class EncoderH264Component : public EncoderComponent
    {
    public:
        EncoderH264Component(const Bitrate& bitrate, const MMAL_PARAMETER_VIDEO_PROFILE_T& profile)
        :   bitrate_(bitrate),
            profile_(profile)
        {
            init(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER);
        }

        void setCallback(EncCallbackData * userData)
        {
            outputPort()->userdata = (struct MMAL_PORT_USERDATA_T *) userData;

            MMAL_STATUS_T status = mmal_port_enable(outputPort(), h264_encoder_buffer_callback);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Failed to setup encoder callback");

            // Send all the buffers to the encoder output port
            unsigned qLen = mmal_queue_length(userData->encoderPool->queue);
            for (unsigned i = 0; i < qLen; i++)
            {
                MMAL_BUFFER_HEADER_T * buffer = mmal_queue_get(userData->encoderPool->queue);
                if (!buffer)
                    vcos_log_error("Unable to get a required buffer %d from pool queue", i);

                MMAL_STATUS_T status = mmal_port_send_buffer(outputPort(), buffer);
                if (status != MMAL_SUCCESS)
                    vcos_log_error("Unable to send a buffer to encoder output port (%d)", i);
            }
        }

    private:
        virtual void setFormat() override
        {
            // Only supporting H264 at the moment
            outputPort()->format->encoding = MMAL_ENCODING_H264;
            outputPort()->format->bitrate = bitrate_.bitrate;
        }

        virtual void setParameters() override
        {
            MMAL_STATUS_T status;
#if 0
            MMAL_PARAMETER_VIDEO_RATECONTROL_T rateParam = {{ MMAL_PARAMETER_RATECONTROL, sizeof(rateParam)}, MMAL_VIDEO_RATECONTROL_DEFAULT};
            status = mmal_port_parameter_set(outputPort(), &rateParam.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set ratecontrol");

            MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, (unsigned)intraperiod};
            status = mmal_port_parameter_set(outputPort(), &param.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set intraperiod");

            status = mmal_port_parameter_set_boolean(inputPort(), MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, MMAL_TRUE);
            if (status != MMAL_SUCCESS)
            {
                vcos_log_error("Unable to set immutable input flag");
                // Continue rather than abort..
            }
#endif
            if (bitrate_.bitrate == 0)
            {
                MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, bitrate_.quantisation};
                status = mmal_port_parameter_set(outputPort(), &param.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set initial QP");

                MMAL_PARAMETER_UINT32_T param2 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)}, bitrate_.quantisation};
                status = mmal_port_parameter_set(outputPort(), &param2.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set min QP");

                MMAL_PARAMETER_UINT32_T param3 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)}, bitrate_.quantisation};
                status = mmal_port_parameter_set(outputPort(), &param3.hdr);
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set max QP");
            }

            status = mmal_port_parameter_set(outputPort(), &profile_.hdr);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set H264 profile");

            //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
            status = mmal_port_parameter_set_boolean(outputPort(), MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, MMAL_TRUE);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set INLINE HEADER FLAG parameters");
        }

    private:
        Bitrate bitrate_;
        MMAL_PARAMETER_VIDEO_PROFILE_T profile_;
    };

#if 0
    ///
    class EncoderJpegComponent : public EncoderComponent
    {
    public:
        static const unsigned JPEG_QUALITY = 75;

        EncoderJpegComponent()
        {
            init(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER);
        }

        void setCallback(EncCallbackData * userData)
        {
            outputPort()->userdata = (struct MMAL_PORT_USERDATA_T *) userData;

            MMAL_STATUS_T status = mmal_port_enable(outputPort(), jpeg_encoder_buffer_callback);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Failed to setup encoder callback");

            // Send all the buffers to the encoder output port
            unsigned qLen = mmal_queue_length(userData->encoderPool->queue);
            for (unsigned i = 0; i < qLen; i++)
            {
                MMAL_BUFFER_HEADER_T * buffer = mmal_queue_get(userData->encoderPool->queue);
                if (!buffer)
                    vcos_log_error("Unable to get a required buffer %d from pool queue", i);

                MMAL_STATUS_T status = mmal_port_send_buffer(outputPort(), buffer);
                if (status != MMAL_SUCCESS)
                    vcos_log_error("Unable to send a buffer to encoder output port (%d)", i);
            }
        }

    private:
        virtual void setFormat() override
        {
            outputPort()->format->encoding = MMAL_ENCODING_JPEG;
        }

        virtual void setParameters() override
        {
            MMAL_STATUS_T status = mmal_port_parameter_set_uint32(outputPort(), MMAL_PARAMETER_JPEG_Q_FACTOR, JPEG_QUALITY);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set jpeg quality");
        }
    };

    ///
    class SplitterComponent
    {
    public:
        SplitterComponent(MMAL_PORT_T * videoPort)
        {
            MMAL_STATUS_T status = mmal_component_create(MMAL_COMPONENT_DEFAULT_SPLITTER, &component_);
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to create encoder component");

            if (component_->input_num < 1 || component_->output_num < 2)
                throw MmalException(MMAL_ENOSYS, "Splitter doesn't have input/output ports");

            mmal_format_copy(inputPort()->format, videoPort->format);
            inputPort()->buffer_num = videoPort->buffer_num;

            status = mmal_port_format_commit(inputPort());
            if (status != MMAL_SUCCESS)
                throw MmalException(status, "Unable to set format on splitter input port");
#if 0
            for (unsigned i=0; i<component_->output_num; ++i)
            {
                mmal_format_copy(outputPort(i)->format, inputPort()->format);

                status = mmal_port_format_commit(outputPort(i));
                if (status != MMAL_SUCCESS)
                    throw MmalException(status, "Unable to set format on splitter output port");
            }
#endif
        }

        ~SplitterComponent()
        {
            for (unsigned i=0; i<component_->output_num; ++i)
            {
                if (outputPort(i) && outputPort(i)->is_enabled)
                    mmal_port_disable(outputPort(i));
            }

            if (component_)
            {
                mmal_component_disable(component_);
                mmal_component_destroy(component_);
            }
        }

        MMAL_PORT_T * inputPort() { return component_->input[0]; }
        MMAL_PORT_T * outputPort(unsigned id) { return component_->output[id]; }

    private:
        MMAL_COMPONENT_T * component_;
    };
#endif

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
        RasPiPreview(bool wantPreview)
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
        RaspberryPiCamera(int camNum, bool wantPreview = false)
        :   cameraNum_(camNum),
            mode_(VideoMode::VM_1080p30),
            isOK_(false)
        {
            //bcm_host_init();
            vcos_log_register("RPiCam", VCOS_LOG_CATEGORY);

            try
            {
                camera_ = std::make_shared<CameraComponent>(cameraNum_, mode_);

                // preview
                {
                    preview_ = std::make_shared<RasPiPreview>(wantPreview);
                    if (wantPreview)
                        connPreview_ = std::make_shared<Connection>(camera_->previewPort(), preview_->inputPort());
                }
#if 0
                // Splitter
                {
                    splitter_ = std::make_shared<SplitterComponent>(camera_->videoPort());
                    connSplitter_ = std::make_shared<Connection>(camera_->videoPort(), splitter_->inputPort());
                }

                // MJPEG encoder
                {
                    encoderJpeg_ = std::make_shared<EncoderJpegComponent>();
                }

                //
                {
                    connEncoderJpeg_ = std::make_shared<Connection>(splitter_->outputPort(1), encoderJpeg_->inputPort());

                    callbackDataJpeg_.encoderPool = encoderJpeg_->pool();
                    encoderJpeg_->setCallback(&callbackDataJpeg_);
                }
#endif
                // H264 encoder
                {
                    MMAL_PARAMETER_VIDEO_PROFILE_T profile;
                    profile.hdr.id = MMAL_PARAMETER_PROFILE;
                    profile.hdr.size = sizeof(profile);
                    profile.profile[0].profile = MMAL_VIDEO_PROFILE_H264_BASELINE;
                    profile.profile[0].level = MMAL_VIDEO_LEVEL_H264_4; // This is the only value supported

                    Bitrate bitrate;

                    encoderH264_ = std::make_shared<EncoderH264Component>(bitrate, profile);
                }

                //
                {
                    connEncoderH264_ = std::make_shared<Connection>(camera_->videoPort(), encoderH264_->inputPort());

                    callbackDataH264_.encoderPool = encoderH264_->pool();
                    encoderH264_->setCallback(&callbackDataH264_);
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
            connEncoderH264_.reset();
#if 0
            connEncoderJpeg_.reset();
            connSplitter_.reset();
#endif
        }

        bool isOK() const { return isOK_; }

        bool startCapture()
        {
            return mmal_port_parameter_set_boolean(camera_->videoPort(), MMAL_PARAMETER_CAPTURE, MMAL_TRUE) == MMAL_SUCCESS;
        }

        bool stopCapture()
        {
            return mmal_port_parameter_set_boolean(camera_->videoPort(), MMAL_PARAMETER_CAPTURE, MMAL_FALSE) == MMAL_SUCCESS;
        }

        bool nextFrame(std::vector<uint8_t>& outData, int64_t& pts, bool& isKey)
        {
            FramePtr frame = callbackDataH264_.nextFrame();
            if (!frame)
                return false;

            frame->swap(outData);
            pts = frame->pts;
            isKey = frame->isKey();
            return true;
        }
#if 0
        bool nextFrameLQ(std::vector<uint8_t>& outData)
        {
            FramePtr frame = callbackDataJpeg_.nextFrame();
            if (!frame)
                return false;

            frame->swap(outData);
            return true;
        }
#endif
        unsigned maxBitrate() const { return 20000; } // kbit/s

        bool resolutionHQ(unsigned num, VideoMode& m)
        {
            switch (num)
            {
                case 0:
                    m = VideoMode(VideoMode::VM_1080p30);
                    return true;
                case 1:
                    m = VideoMode(VideoMode::VM_720p50);
                    return true;
                case 2:
                    m = VideoMode(VideoMode::VM_640x480p60);
                    return true;
                case 3:
                    m = VideoMode(VideoMode::VM_640x480p90);
                    return true;
            };

            return false;
        }

        bool resolutionLQ(unsigned num, VideoMode& m)
        {
            switch (num)
            {
                case 0:
                    m = VideoMode(VideoMode::JM_480x270);
                    return true;
            };

            return false;
        }

        // TODO
        bool setResolution(unsigned width, unsigned height)
        {
            VideoMode m(VideoMode::VM_1080p30);
            if (width == m.width && height == m.height)
                return true;
            return false;
        }

    private:
        unsigned cameraNum_;
        VideoMode mode_;
        EncCallbackData callbackDataH264_;

        std::shared_ptr<CameraComponent> camera_;
        std::shared_ptr<RasPiPreview> preview_;
        std::shared_ptr<EncoderH264Component> encoderH264_;

        std::shared_ptr<Connection> connPreview_;
        std::shared_ptr<Connection> connEncoderH264_;
#if 0
        EncCallbackData callbackDataJpeg_;
        std::shared_ptr<SplitterComponent> splitter_;
        std::shared_ptr<EncoderJpegComponent> encoderJpeg_;
        std::shared_ptr<Connection> connSplitter_;
        std::shared_ptr<Connection> connEncoderJpeg_;
#endif
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
static void h264_encoder_buffer_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer)
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
        vcos_log_error("Received a encoder buffer callback with no state");

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

#if 0
/// buffer header callback function for encoder
///
/// @param port Pointer to port from which callback originated
/// @param buffer mmal buffer header pointer
///
static void jpeg_encoder_buffer_callback(MMAL_PORT_T * port, MMAL_BUFFER_HEADER_T * buffer)
{
    static const unsigned MAX_QUEUE_SIZE = 32;

    rpi_cam::EncCallbackData * pData = (rpi_cam::EncCallbackData *)port->userdata;

    if (buffer->cmd)
        vcos_log_error("encoder callback: received unexpected callback event, 0x%08x", buffer->cmd);

    if (pData)
    {
        mmal_buffer_header_mem_lock(buffer); // BUFFER LOCK

        pData->curFrame.insert(pData->curFrame.end(), buffer->data, buffer->data + buffer->length);

        mmal_buffer_header_mem_unlock(buffer); // BUFFER UNLOCK

        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
        {
            std::lock_guard<std::mutex>( pData->queueMutex ); // LOCK

            while (pData->queue.size() >= MAX_QUEUE_SIZE)
                pData->queue.pop_front();

            pData->queue.push_back( std::make_shared<rpi_cam::Frame>(pData->curFrame, 0) );
            pData->curFrame.clear();
        }
    }
    else
        vcos_log_error("Received a encoder buffer callback with no state");

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
#endif


#endif // RASPBERRY_PI_CAMERA
