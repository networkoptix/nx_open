// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <usrsctp.h>

#include "webrtc_utils.h"

namespace nx::webrtc {

class AbstractDataChannelDelegate;

class NX_WEBRTC_API DataChannel
{
public:
    /**
     * Messages for the DataChannel establishment protocol (RFC 8832).
     * See https://www.rfc-editor.org/rfc/rfc8832.html
     */
    enum MessageType: uint8_t
    {
        openRequest = 0x00, /**< Unused.*/
        openResponse = 0x01, /**< Unused.*/
        ack = 0x02,
        open = 0x03,
        close = 0x04,
    };

    enum ChannelType: uint8_t
    {
        reliable = 0x00,
        partialReliableRexmit = 0x01,
        partialReliableTimed = 0x02,
    };

private:
    /**
     * NOTE: The order seems to be wrong but these are the actual values.
     * See https://datatracker.ietf.org/doc/html/draft-ietf-rtcweb-data-channel-13#section-8
     */
    enum PayloadId: uint32_t
    {
        control = 50,
        string = 51,
        binaryPart = 52,
        binary = 53,
        stringPart = 54,
        stringEmpty = 56,
        binaryEmpty = 57,
    };

    struct Message
    {
        enum class Type
        {
            string,
            binary,
            control,
            reset,
        };
        Type type = Type::control;
        int streamId = 0;
        std::string data;
        bool eor = true;
    };

public:
    DataChannel(
        AbstractDataChannelDelegate* ice,
        uint16_t sctpPort,
        const SessionConfig& config);
    ~DataChannel();
    bool handlePacket(const uint8_t* data, int size); /**< Input from the lower transport. */
    bool writeString(
        const std::string& data,
        int streamId = -1); /**< Output into Data Channel transport. */
    bool writeBinary(
        const std::string& data,
        int streamId = -1); /**< Output into Data Channel transport. */
    bool openDataChannel(
        const std::string& label,
        const std::string& protocol); /**< Open Data Channel. */
    bool closeStream(int streamId = -1); /**< Close stream. */
    void reset(); /**< Remove lower transport. */
    int defaultStream() const { return m_defaultStreamId; }
    int outputQueueSize() const;

private:
    bool init();
    bool handleIo();
    bool handleRead();
    bool handleWrite();
    bool processReceivedData(int sid, PayloadId ppid);
    bool processReceivedMessage(const Message& message);
    bool processNotification(const union sctp_notification* notify, int len);
    bool processOpenMessage(const Message& message);
    bool sendStreamReset(int streamId);
    bool writeMessage(Message message);
    bool writeUserMessage(Message&& message); /**< Used by writeString() and writeBinary(). */
    bool flushSavedMessages();
    static void usrsctpDebugCallback(const char* format, ...);
    static int usrsctpWriteCallback(
        void* ptr,
        void* data,
        size_t len,
        uint8_t tos,
        uint8_t set_df);
    static void usrsctpUpcallCallback(struct socket*, void* arg, int flags);

private:
    AbstractDataChannelDelegate* m_ice = nullptr;
    uint16_t m_sctpPort = 0;
    const SessionConfig m_config;
    struct socket* m_sctpSocket = nullptr;
    int m_lastUpcallEvents = 0;
    std::vector<uint8_t> m_receiveBuffer;
    std::string m_lastMessage;
    std::string m_partialString;
    std::string m_partialBinary;
    std::string m_partialNotification;
    std::optional<uint16_t> m_negotiatedStreamCount;
    bool m_dataChannelOpened = false;
    int m_defaultStreamId = 0;
    std::string m_label;
    std::string m_protocol;
    std::queue<Message> m_outputQueue;
    std::queue<Message> m_savedQueue;
};

class NX_WEBRTC_API AbstractDataChannelDelegate
{
public:
    AbstractDataChannelDelegate(const SessionConfig& config);
    virtual ~AbstractDataChannelDelegate();
    virtual void writeDataChannelPacket(const uint8_t* data, int size) = 0;
    virtual void onDataChannelString(const std::string& data, int streamId) = 0;
    virtual void onDataChannelBinary(const std::string& data, int streamId) = 0;

protected:
    DataChannel m_dataChannel;
};

} // namespace nx::webrtc
