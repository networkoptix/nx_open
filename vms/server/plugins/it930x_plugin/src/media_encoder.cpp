#include "ref_counter.h"
#include "rx_device.h"

#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"

namespace
{
    void validate(nxcip::LiveStreamConfig& config, const nxcip::LiveStreamConfig& prev)
    {
        static const int MIN_BITRATE_KBPS = 200;

        if (config.width <= 0 || config.height <= 0)
        {
            config.width = prev.width;
            config.height = prev.height;
        }

        if (config.framerate <= 0)
            config.framerate = prev.framerate;

        if (config.bitrateKbps < 0)
            config.bitrateKbps = prev.bitrateKbps;
        if (config.bitrateKbps < MIN_BITRATE_KBPS)
            config.bitrateKbps = MIN_BITRATE_KBPS;
    }
}

namespace ite
{
    MediaEncoder::MediaEncoder(CameraManager * const cameraManager, int encoderNumber)
    :   m_cameraManager(cameraManager),
        m_encoderNumber(encoderNumber)
    {
    }

    MediaEncoder::~MediaEncoder()
    {
    }

    void * MediaEncoder::queryInterface(const nxpl::NX_GUID& interfaceID)
    {
        if (interfaceID == nxcip::IID_CameraMediaEncoder3)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder3*>(this);
        }
        if (interfaceID == nxcip::IID_CameraMediaEncoder2)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder2*>(this);
        }
        if (interfaceID == nxcip::IID_CameraMediaEncoder)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder*>(this);
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }

        return nullptr;
    }

    int MediaEncoder::getMediaUrl(char * ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::getResolutionList(nxcip::ResolutionInfo * infoList, int * infoListCount) const
    {
        nxcip::ResolutionInfo res;

        std::lock_guard<std::mutex> lock(m_cameraManager->get_mutex());
        if (!m_cameraManager->rxDeviceRef())
            return nxcip::NX_NO_DATA;

        std::shared_ptr<RxDevice> rxDev = m_cameraManager->rxDevice().lock();
        if (rxDev)
            rxDev->resolution(m_encoderNumber, res.resolution.width, res.resolution.height, res.maxFps);
        else
            RxDevice::resolutionPassive(m_encoderNumber, res.resolution.width, res.resolution.height, res.maxFps);

        infoList[0] = res;
        *infoListCount = 1;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getMaxBitrate(int * maxBitrate) const
    {
        static const unsigned MAX_BITRATE_KBPS = 12000;

        *maxBitrate = MAX_BITRATE_KBPS;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setResolution(const nxcip::Resolution& )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::setFps(const float& , float * )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::setBitrate(int , int * )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::getAudioFormat( nxcip::AudioFormat* /*audioFormat*/ ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::getVideoFormat(nxcip::CompressionType * codec, nxcip::PixelFormat * pixelFormat) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    nxcip::StreamReader * MediaEncoder::getLiveStreamReader()
    {
        std::lock_guard<std::mutex> lock(m_cameraManager->get_mutex());
        if (!m_cameraManager->rxDeviceRef() || !m_cameraManager->rxDeviceRef()->deviceReady())
            return nullptr;

        StreamReader * stream = new StreamReader(m_cameraManager, m_encoderNumber);

        debug_printf(
            "[MediaEncoder::getLiveStreamReader] trying to get a stream reader from Rx DONE!\n");

        return stream;
    }

    int MediaEncoder::getConfiguredLiveStreamReader(nxcip::LiveStreamConfig * config, nxcip::StreamReader ** ppStream)
    {
        if (!config)
            return nxcip::NX_INVALID_PARAM_VALUE;

        std::unique_lock<std::mutex> lock(m_cameraManager->get_mutex());
        if (!m_cameraManager->rxDeviceRef() || !m_cameraManager->rxDeviceRef()->deviceReady())
            return nxcip::NX_NO_DATA;

        *ppStream = nullptr;
        RxDevicePtr rxDev = m_cameraManager->rxDevice().lock();

        debug_printf(
            "[MediaEncoder::getConfiguredLiveStreamReader] trying to get a stream reader from Rx %d\n",
            rxDev->rxID()
        );

        if (!rxDev || !rxDev->deviceReady())
            return nxcip::NX_OTHER_ERROR;

#if 0 // Open the stream any case (non-configured).
        // config in first arrived stream, another is locked
        if (rxDev->configureTx()) // delay inside
        {
            debug_printf(
                "[MediaEncoder::getConfiguredLiveStreamReader] Rx: %d. Tx configured successfully\n",
                rxDev->rxID()
            );

            nxcip::LiveStreamConfig curConfig;
            memset(&curConfig, 0, sizeof(nxcip::LiveStreamConfig));
            //curConfig.codec = nxcip::CODEC_ID_H264;

            if (! rxDev->getEncoderParams(m_encoderNumber, curConfig))
                return nxcip::NX_TRY_AGAIN;

            validate(*config, curConfig);

            debug_printf(
                "[MediaEncoder::getConfiguredLiveStreamReader] Rx: %d. setting encoder params\n",
                rxDev->rxID()
            );

            if (curConfig.framerate != config->framerate ||
                curConfig.bitrateKbps != config->bitrateKbps)
            {
                if (! rxDev->setEncoderParams(m_encoderNumber, *config))
                    return nxcip::NX_INVALID_PARAM_VALUE;
            }
        }
        else
            return nxcip::NX_TRY_AGAIN;
#endif

        lock.unlock();
        *ppStream = getLiveStreamReader();
        if (*ppStream == nullptr)
            return nxcip::NX_OTHER_ERROR;
        return nxcip::NX_NO_ERROR;
    }
}
