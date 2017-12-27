#pragma once

#include <atomic>

#include <QtCore/QUrl>
#include <QtCore/QIODevice>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include <nx/network/abstract_socket.h>
#include <utils/memory/cyclic_allocator.h>

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

} // extern "C"

class GenericMulticastStreamReader: public nxcip::StreamReader
{
    struct Extras
    {
        QByteArray extradata;
    };

public:

    GenericMulticastStreamReader(const QUrl& url);
    virtual ~GenericMulticastStreamReader();

    /** Implementation of nxpl::PluginInterface::queryInterface */
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    /** Implementation of nxpl::PluginInterface::addRef */
    virtual unsigned int addRef() override;
    /** Implementation of nxpl::PluginInterface::releaseRef */
    virtual unsigned int releaseRef() override;

    /** Implementation of nxcip::StreamReader::getNextData */
    virtual int getNextData(nxcip::MediaDataPacket** outPacket) override;
    /** Implementation of nxcip::StreamReader::getNextData */
    virtual void interrupt() override;

    bool open();
    void close();

    void setAudioEnabled(bool audioEnabled);
    void resetAudioFormat();
    nxcip::AudioFormat audioFormat() const;


private:

    bool initLayout();
    bool preprocessPacket(
        const AVPacket& packet,
        uint8_t** data,
        int* dataSize,
        Extras* outExtras);

    bool hasAdtsHeader(const AVPacket& packet);
    bool removeAdtsHeaderAndFillExtradata(
        const AVPacket& packet,
        uint8_t** dataPointer,
        int* dataSize,
        Extras* outExtras);

    bool isPacketOk(const AVPacket& packet) const;
    bool isPacketStreamOk(const AVPacket& packet) const;
    bool isPacketDataTypeOk(const AVPacket& packet) const;
    bool isPacketTimestampOk(const AVPacket& packet) const;

    nxcip::UsecUTCTimestamp packetTimestamp(const AVPacket& packet) const;
    unsigned int packetChannelNumber(const AVPacket& packet) const;
    unsigned int packetFlags(const AVPacket& packet) const;
    nxcip::DataPacketType packetDataType(const AVPacket& packet) const;

    int64_t currentTimeSinceEpochUs() const;

private:
    nxpt::CommonRefManager m_refManager;
    std::atomic<bool> m_interrupted = {false};
    QUrl m_url;
    CyclicAllocator m_allocator;

    AVFormatContext* m_formatContext = nullptr;
    std::map<int64_t, int64_t> m_streamIndexToChannelNumber;
    int m_firstVideoIndex = 0;

    nxcip::AudioFormat m_audioFormat;
    bool m_audioEnabled = false;
};
