#include <cstring>

#include <QtCore/QUrl>

#include "generic_multicast_media_encoder.h"
#include "generic_multicast_camera_manager.h"
#include "generic_multicast_stream_reader.h"

GenericMulticastMediaEncoder::GenericMulticastMediaEncoder(
    GenericMulticastCameraManager* const cameraManager)
:
    m_refManager(this),
    m_cameraManager(cameraManager)
{
}

GenericMulticastMediaEncoder::~GenericMulticastMediaEncoder()
{
}

void* GenericMulticastMediaEncoder::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (memcmp(&interfaceId, &nxcip::IID_CameraMediaEncoder, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder*>(this);
    }
    if (memcmp(&interfaceId, &nxcip::IID_CameraMediaEncoder2, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder2*>(this);
    }
    if (memcmp(&interfaceId, &nxpl::IID_PluginInterface, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }

    return nullptr;
}

unsigned int GenericMulticastMediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericMulticastMediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int GenericMulticastMediaEncoder::getMediaUrl(char* urlBuf) const
{
    strcpy(urlBuf, m_cameraManager->info().url);
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::getResolutionList(nxcip::ResolutionInfo* /*infoList*/, int* infoListCount) const
{
    *infoListCount = 0;
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::getMaxBitrate(int* maxBitrate) const
{
    *maxBitrate = 0;
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::setResolution(const nxcip::Resolution& /*resolution*/)
{
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::setFps(const float& /*fps*/, float* /*selectedFps*/)
{
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastMediaEncoder::setBitrate(int /*bitrateKbps*/, int* /*selectedBitrateKbps*/)
{
    return nxcip::NX_NO_ERROR;
}

nxcip::StreamReader* GenericMulticastMediaEncoder::getLiveStreamReader()
{
    qDebug() << "getting live stream reader!";

    if (!m_streamReader)
    {
        auto url = QUrl(m_cameraManager->info().url);
        m_streamReader = std::make_unique<GenericMulticastStreamReader>(url);
    }

    if (!m_streamReader->open())
    {
        qDebug() << "Unable to open stream!";
        return nullptr;
    }

    m_streamReader->setAudioEnabled(m_cameraManager->isAudioEnabled());
    m_streamReader->addRef();
    return m_streamReader.get();
}

int GenericMulticastMediaEncoder::getAudioFormat(nxcip::AudioFormat* audioFormat) const
{
    if (!m_streamReader)
        return nxcip::NX_TRY_AGAIN;

    *audioFormat = m_streamReader->audioFormat();
    return nxcip::NX_NO_ERROR;
}
