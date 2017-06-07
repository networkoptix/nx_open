#pragma once

#include <QtCore/QByteArray>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <utils/memory/cyclic_allocator.h>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

class GenericMulticastMediaPacket: public nxcip::MediaDataPacket2
{

public:
    GenericMulticastMediaPacket(CyclicAllocator* allocator);
    virtual ~GenericMulticastMediaPacket();

    /** Implementation of nxpl::PluginInterface::queryInterface */
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    /** Implementation of nxpl::PluginInterface::addRef */
    virtual unsigned int addRef() override;
    /** Implementation of nxpl::PluginInterface::releaseRef */
    virtual unsigned int releaseRef() override;

    /** Implementation of nxcip::MediaDataPacket::timestamp */
    virtual nxcip::UsecUTCTimestamp timestamp() const override;

    /** Implementation of nxcip::MediaDataPacket::type */
    virtual nxcip::DataPacketType type() const override;

    /** Implementation of nxcip::MediaDataPacket::data */
    virtual const void* data() const override;
    
    /** Implementation of nxcip::MediaDataPacket::dataSize */
    virtual unsigned int dataSize() const override;

    /** Implementation of nxcip::MediaDataPacket::channelNumber */
    virtual unsigned int channelNumber() const override;

    /** Implementation of nxcip::MediaDataPacket::codecType */
    virtual nxcip::CompressionType codecType() const override;
    
    /** Implementation of nxcip::MediaDataPacket::flags */
    virtual unsigned int flags() const override;
    
    /** Implementation of nxcip::MediaDataPacket::cSeq */
    virtual unsigned int cSeq() const override;

    /** Implemantation of nxcip::MediaDataPacvet2::extradata*/
    virtual const char* extradata() const override;

    /** Implemantation of nxcip::MediaDataPacvet2::extradata*/
    virtual int extradataSize() const override;

    void setTimestamp(nxcip::UsecUTCTimestamp timestamp);

    void setType(nxcip::DataPacketType dataPacketType);

    void setChannelNumber(unsigned int channelNumber);

    void setCodecType(nxcip::CompressionType codecType);

    void setCodecType(AVCodecID codecType);

    void setFlags(unsigned int flags);

    void setCSeq(unsigned int cSeq);

    void setData(uint8_t* data, int64_t dataSize);

    void setExtradata(const QByteArray& extradata);

    void setExtradata(const char* const extradata, int extradataSize);

private:
    nxpt::CommonRefManager m_refManager;
    uint8_t* m_buffer = nullptr;
    int64_t m_dataSize = 0;
    int64_t m_bufferSize = 0;
    CyclicAllocator* m_allocator = nullptr;
    nxcip::UsecUTCTimestamp m_timestamp = nxcip::INVALID_TIMESTAMP_VALUE;
    nxcip::DataPacketType m_dataPacketType = nxcip::DataPacketType::dptEmpty;
    unsigned int m_channelNumber = 0;
    nxcip::CompressionType m_codecType = nxcip::CompressionType::AV_CODEC_ID_NONE;
    unsigned int m_flags = 0;
    unsigned int m_cSeq = 0;
    QByteArray m_extradata;

};