#include <cstring>

#include <QString>
#include <QStringList>

#include "generic_multicast_camera_manager.h"
#include "generic_multicast_media_encoder.h"
#include "generic_multicast_stream_reader.h"

namespace
{

const char* multicastPluginParamsXML =
    "<?xml version=\"1.0\"?>                                          \
    <plugin                                                           \
            name=\"GenericMulticastPlugin\"                           \
            version=\"1\"                                             \
            unique_id=\"5f41750e-d2f2-4056-928e-a01a833cceae\">       \
        <parameters>                                                  \
            <group name=\"Main\">                                     \
                <param                                                \
                    name=\"multicast_url\"                            \
                    description=\"Multicast URL\"                     \
                    dataType=\"String\"                               \
                    readOnly=\"true\"/>                               \
            </group>                                                  \
        </parameters>                                                 \
    </plugin>";

} // namespace

GenericMulticastCameraManager::GenericMulticastCameraManager(const nxcip::CameraInfo& info)
:
    m_refManager(this),
    m_pluginRef(GenericMulticastPlugin::instance()),
    m_info(info),
    m_capabilities(0)
{
    m_capabilities
        |= nxcip::BaseCameraManager::audioCapability
        | nxcip::BaseCameraManager::shareIpCapability
        | nxcip::BaseCameraManager::primaryStreamSoftMotionCapability
        | nxcip::BaseCameraManager::nativeMediaStreamCapability
        | nxcip::BaseCameraManager::relativeTimestampCapability;
}

GenericMulticastCameraManager::~GenericMulticastCameraManager()
{
}

void* GenericMulticastCameraManager::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    if (memcmp(&interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager)) == 0)
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager*>(this);
    }
    if (memcmp(&interfaceID, &nxcip::IID_BaseCameraManager2, sizeof(nxcip::IID_BaseCameraManager2)) == 0)
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager2*>(this);
    }
    if (memcmp(&interfaceID, &nxcip::IID_BaseCameraManager3, sizeof(nxcip::IID_BaseCameraManager3)) == 0)
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager3*>(this);
    }

    return nullptr;
}

unsigned int GenericMulticastCameraManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericMulticastCameraManager::releaseRef()
{
    return m_refManager.releaseRef();
}

int GenericMulticastCameraManager::getEncoderCount(int* encoderCount) const
{
    *encoderCount = 1;
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastCameraManager::getEncoder(int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr)
{
    if (encoderIndex != 0)
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if (!m_encoder.get())
        m_encoder.reset(new GenericMulticastMediaEncoder(this));

    m_encoder->addRef();
    *encoderPtr = m_encoder.get();

    return nxcip::NX_NO_ERROR;
}

int GenericMulticastCameraManager::getCameraInfo(nxcip::CameraInfo* info) const
{
    memcpy(info, &m_info, sizeof(m_info));
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastCameraManager::getCameraCapabilities(unsigned int* capabilitiesMask) const
{
    *capabilitiesMask = m_capabilities;
    return nxcip::NX_NO_ERROR;
}

void GenericMulticastCameraManager::setCredentials(const char* /*username*/, const char* /*password*/)
{
    // Do nothing.
}

int GenericMulticastCameraManager::setAudioEnabled(int audioEnabled)
{
    m_audioEnabled = audioEnabled;
    return 0;
}

nxcip::CameraPtzManager *GenericMulticastCameraManager::getPtzManager() const
{
    return NULL;
}

nxcip::CameraMotionDataProvider* GenericMulticastCameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

nxcip::CameraRelayIOManager* GenericMulticastCameraManager::getCameraRelayIOManager() const
{
    return NULL;
}

void GenericMulticastCameraManager::getLastErrorString(char* errorString) const
{
    errorString[0] = '\0';
}

int GenericMulticastCameraManager::createDtsArchiveReader(nxcip::DtsArchiveReader** /*dtsArchiveReader*/) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int GenericMulticastCameraManager::find(nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int GenericMulticastCameraManager::setMotionMask(nxcip::Picture* /*motionMask*/)
{
    return nxcip::NX_NOT_IMPLEMENTED;
}


const char* GenericMulticastCameraManager::getParametersDescriptionXML() const
{
    return multicastPluginParamsXML;
}

int GenericMulticastCameraManager::getParamValue(const char* paramName, char* valueBuf, int* valueBufSize) const
{
    if (strcmp(paramName, "/main/multicast_url") == 0)
    {
        const int requiredBufSize = strlen(m_info.url) + 1;
        if (*valueBufSize < requiredBufSize)
        {
            *valueBufSize = requiredBufSize;
            return nxcip::NX_MORE_DATA;
        }

        *valueBufSize = requiredBufSize;
        strcpy(valueBuf, m_info.url);
        return nxcip::NX_NO_ERROR;
    }

    return nxcip::NX_UNKNOWN_PARAMETER;
}

int GenericMulticastCameraManager::setParamValue(const char* paramName, const char* /*value*/)
{
    if (strcmp(paramName, "/main/multicast_url") == 0)
        return nxcip::NX_PARAM_READ_ONLY;

    return nxcip::NX_UNKNOWN_PARAMETER;
}


const nxcip::CameraInfo& GenericMulticastCameraManager::info() const
{
    return m_info;
}

nxpt::CommonRefManager* GenericMulticastCameraManager::refManager()
{
    return &m_refManager;
}

bool GenericMulticastCameraManager::isAudioEnabled() const
{
    return m_audioEnabled;
}
