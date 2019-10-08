#include "generic_multicast_media_packet.h"
#include "utils.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

#include <nx/kit/utils.h>

GenericMulticastMediaPacket::GenericMulticastMediaPacket():
    m_refManager(this)
{

}

GenericMulticastMediaPacket::~GenericMulticastMediaPacket()
{
    if (m_buffer)
    {
        nx::kit::utils::freeAligned(m_buffer);

        m_buffer = nullptr;
        m_bufferSize = 0;
        m_dataSize = 0;
    }
}

void* GenericMulticastMediaPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (memcmp(&interfaceId, &nxcip::IID_MediaDataPacket, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxcip::MediaDataPacket*>(this);
    }
    if (memcmp(&interfaceId, &nxcip::IID_MediaDataPacket2, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxcip::MediaDataPacket2*>(this);
    }
    if (memcmp(&interfaceId, &nxpl::IID_PluginInterface, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }

    return nullptr;
}

int GenericMulticastMediaPacket::addRef() const
{
    return m_refManager.addRef();
}

int GenericMulticastMediaPacket::releaseRef() const
{
    return m_refManager.releaseRef();
}

nxcip::UsecUTCTimestamp GenericMulticastMediaPacket::timestamp() const
{
    return m_timestamp;
}

nxcip::DataPacketType GenericMulticastMediaPacket::type() const
{
    return m_dataPacketType;
}

const void* GenericMulticastMediaPacket::data() const
{
    return m_buffer;
}

unsigned int GenericMulticastMediaPacket::dataSize() const
{
    return m_dataSize;
}

unsigned int GenericMulticastMediaPacket::channelNumber() const
{
    return m_channelNumber;
}

nxcip::CompressionType GenericMulticastMediaPacket::codecType() const
{
    return m_codecType;
}

unsigned int GenericMulticastMediaPacket::flags() const
{
    return m_flags;
}

unsigned int GenericMulticastMediaPacket::cSeq() const
{
    return m_cSeq;
}


const char* GenericMulticastMediaPacket::extradata() const
{
    return m_extradata.constData();
}

int GenericMulticastMediaPacket::extradataSize() const
{
    return m_extradata.size();
}

void GenericMulticastMediaPacket::setTimestamp(nxcip::UsecUTCTimestamp timestamp)
{
    m_timestamp = timestamp;
}

void GenericMulticastMediaPacket::setType(nxcip::DataPacketType dataPacketType)
{
    m_dataPacketType = dataPacketType;
}

void GenericMulticastMediaPacket::setChannelNumber(unsigned int channelNumber)
{
    m_channelNumber = channelNumber;
}

void GenericMulticastMediaPacket::setCodecType(nxcip::CompressionType codecType)
{
    m_codecType = codecType;
}

void GenericMulticastMediaPacket::setCodecType(AVCodecID codecType)
{
    m_codecType = fromFfmpegCodecIdToNx(codecType);
}

void GenericMulticastMediaPacket::setFlags(unsigned int flags)
{
    m_flags = flags;
}

void GenericMulticastMediaPacket::setCSeq(unsigned int cSeq)
{
    m_cSeq = cSeq;
}

void GenericMulticastMediaPacket::setData(uint8_t* data, int64_t dataSize)
{
    NX_ASSERT(dataSize > 0, lm("Wrong data size"));
    if (dataSize <= 0)
        return;

    if (!m_buffer)
    {
        m_buffer = static_cast<uint8_t*>(
            nx::kit::utils::mallocAligned(
                dataSize + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE,
                nxcip::MEDIA_DATA_BUFFER_ALIGNMENT));

        if (!m_buffer)
        {
            m_bufferSize = 0;
            m_dataSize = 0;
        }

        m_bufferSize = dataSize;
    }

    if (dataSize <= m_bufferSize && dataSize > 0)
    {
        memcpy(m_buffer, data, dataSize);
        m_dataSize = dataSize;
        return;
    }

    nx::kit::utils::freeAligned(m_buffer);

    m_buffer = static_cast<uint8_t*>(
        nx::kit::utils::mallocAligned(
            dataSize + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE,
            nxcip::MEDIA_DATA_BUFFER_ALIGNMENT));

    if (!m_buffer)
    {
        m_bufferSize = 0;
        m_dataSize = 0;
    }

    m_bufferSize = dataSize;
    m_dataSize = dataSize;

    memcpy(m_buffer, data, dataSize);
}

void GenericMulticastMediaPacket::setExtradata(const QByteArray& extradata)
{
    m_extradata = extradata;
}

void GenericMulticastMediaPacket::setExtradata(const char* const extradata, int extradataSize)
{
    m_extradata.resize(extradataSize);
    memcpy(m_extradata.data(), extradata, extradataSize);
}
