#ifndef RETURN_CHANNEL_CMD_X_H
#define RETURN_CHANNEL_CMD_X_H

#include <vector>
#include <cstdint>

namespace ite
{
    ///
    class RCCommand
    {
    public:
        enum
        {
            RC_MAX_SIZE = 169,  // if RC command is bigger it would be split
            CMD_MAX_SIZE = 184, // leading ('#') + head (13 bytes) + [RC] + ending (0x0d)
            TS_SIZE = 188       // + TS header: sync (0x47), TEI + PID, TS seq
        };

        static constexpr uint8_t HEAD_SIZE() { return 13; }
        static constexpr uint8_t leadingTag() { return '#'; }
        static constexpr uint8_t endingTag() { return 0x0d; }

        RCCommand(uint8_t * ptr, uint8_t size = CMD_MAX_SIZE)
        :   m_data(ptr),
            m_size(size)
        {}

        static uint16_t in2outID(uint16_t cmdID);

        bool hasData() const { return m_data && m_size; }
        bool hasLeadingTag() const { return m_data[0] == leadingTag(); }
        bool hasEndingTag() const { return m_data[m_size-1] == endingTag(); }
        bool hasTags() const { return hasLeadingTag() && hasEndingTag(); }
        bool isOK() const { return hasData() && hasTags() && checksumOK(); }

        // header bytes
        uint16_t rxID() const           { return (m_data[1]<<8) | m_data[2]; }
        uint16_t txID() const           { return (m_data[3]<<8) | m_data[4]; }
        uint16_t totalPktNum() const    { return (m_data[5]<<8) | m_data[6]; }
        uint16_t paketNum() const       { return (m_data[7]<<8) | m_data[8]; }
        uint16_t sequenceNum() const    { return (m_data[9]<<8) | m_data[10]; }
        uint16_t pktLength() const      { return (m_data[11]<<8) | m_data[12]; }
        // cmd bytes
        uint16_t commandID() const      { return (m_data[17]<<8) | m_data[18]; }

        static uint8_t checksum(const uint8_t * buffer, unsigned length);
        uint8_t calcChecksum() const;
        uint8_t checksumValue() const { return m_data[HEAD_SIZE() + pktLength()]; }
        bool checksumOK() const { return checksumValue() == calcChecksum(); }

        bool isBroadcast() const { return txID() == 0xFFFF; }

        const uint8_t * head() const { return m_data; }
        const uint8_t * content() const { return &m_data[HEAD_SIZE()]; }

        //

        unsigned headCheck() const;

        bool checkPacketCount();

        //

        size_t rawSize() const { return m_size; }
        uint8_t * rawData() { return m_data; }
        const uint8_t * rawData() const { return m_data; }

    private:
        uint8_t * m_data;
        uint8_t m_size;
    };

    ///
    class RCCmdBuffer
    {
    public:
        RCCmdBuffer()
        {
            m_buffer.reserve(RCCommand::CMD_MAX_SIZE);
        }

        unsigned append(const uint8_t * data, unsigned len)
        {
            for (unsigned i = 0; len > 0; ++i, --len)
                m_buffer.push_back(data[i]);
            return len;
        }

        RCCommand cmd()
        {
            if (m_buffer.size())
                return RCCommand(&m_buffer[0], m_buffer.size());
            return RCCommand(nullptr, 0);
        }

    private:
        std::vector<uint8_t> m_buffer;
    };

    ///
    class RebuiltCmd
    {
    public:
        RebuiltCmd()
        :   expectedSize_(0),
            expectedPackets_(0),
            packets_(0),
            isValid_(false)
        {}

        unsigned add(const RCCommand&);

        const uint8_t * data() const { return &cmd_[0]; }
        unsigned size() const { return cmd_.size(); }
        bool isOK() const { return cmd_.size() == expectedSize_; }

        void clear()
        {
            cmd_.clear();
            expectedSize_ = expectedPackets_ = packets_ = 0;
            isValid_ = false;
        }

        bool isValid() const { return isValid_; }
        void setValid(bool value = true) { isValid_ = value; }

        uint16_t commandID() const { return (cmd_[4]<<8) | cmd_[5]; }
        uint8_t returnCode() const { return cmd_[6]; }

    private:
        std::vector<uint8_t> cmd_;
        unsigned expectedSize_;
        unsigned expectedPackets_;
        unsigned packets_;
        bool isValid_;
    };
}

#endif
