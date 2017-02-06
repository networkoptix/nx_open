#ifndef RETURN_CHANNEL_CMD_X_H
#define RETURN_CHANNEL_CMD_X_H

#include <vector>
#include <cstdint>
#include <cstring>

namespace ite
{
    ///
    class RcPacket
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

        RcPacket(uint8_t * ptr, uint8_t size = CMD_MAX_SIZE)
        :   m_data(ptr),
            m_size(size)
        {}

        bool hasData() const { return m_data && m_size; }
        bool hasLeadingTag() const { return m_data[0] == leadingTag(); }
        bool hasEndingTag() const { return m_data[m_size-1] == endingTag(); }
        bool hasTags() const { return hasLeadingTag() && hasEndingTag(); }
        bool validLength() const { return pktLength() <= RC_MAX_SIZE; }
        bool isOK() const { return hasData() && hasTags() && validLength() && checksumOK(); }

        // header bytes
        uint16_t rxID() const           { return get16(1); }
        uint16_t txID() const           { return get16(3); }
        uint16_t totalPktNum() const    { return get16(5); }
        uint16_t paketNum() const       { return get16(7); }
        uint16_t sequenceNum() const    { return get16(9); }
        uint16_t pktLength() const      { return get16(11); }

        void setRxID(uint16_t rxID)         { set16(1, rxID); }
        void setTxID(uint16_t txID)         { set16(3, txID); }
        void setTotalPktNum(uint16_t num)   { set16(5, num); }
        void setPaketNum(uint16_t num)      { set16(7, num); }
        void setSequenceNum(uint16_t num)   { set16(9, num); }
        void setPktLength(uint16_t len)     { set16(11, len); }

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

        //

        void setTags()
        {
            m_data[0] = leadingTag();
            m_data[m_size-1] = endingTag();
        }

        void setContent(const uint8_t * cmd, unsigned length)
        {
            for (unsigned i = 0; i < length; ++i)
                m_data[HEAD_SIZE() + i] = cmd[i];
        }

        void setChecksum()
        {
            m_data[HEAD_SIZE() + pktLength()] = calcChecksum();
        }

        void print() const;

    private:
        uint8_t * m_data;
        uint8_t m_size;

        uint16_t get16(uint8_t pos) const
        {
            return (m_data[pos]<<8) | m_data[pos+1];
        }

        void set16(uint8_t pos, uint16_t val)
        {
            m_data[pos] = (uint8_t)(val>>8);
            m_data[pos+1] = (uint8_t)val;
        }
    };

    ///
    class RcPacketBuffer
    {
    public:
        enum
        {
            CMD_SIZE = RcPacket::CMD_MAX_SIZE,
            BUF_SIZE = RcPacket::TS_SIZE
        };

        static constexpr unsigned CMD_SHIFT() { return BUF_SIZE - CMD_SIZE; }

        void copyTS(const uint8_t * data, unsigned len)
        {
            memcpy(m_buffer, data, len);
        }

        void zero()
        {
            memset(m_buffer, 0, BUF_SIZE);
        }

        RcPacket packet()
        {
            return RcPacket(m_buffer + CMD_SHIFT(), CMD_SIZE);
        }

        uint8_t * tsBuffer() { return m_buffer; }

        //

        bool checkbit() const { return m_buffer[1] & 0x80; }   // Transport Error Indicator

        uint16_t pid() const
        {
            uint16_t x = (m_buffer[1] & 0x1F);
            x <<= 8;
            x |= m_buffer[2];
            return x;
        }

    private:
        uint8_t m_buffer[BUF_SIZE];
    };

    struct SendSequence;

    ///
    class RcCommand
    {
    public:
        enum
        {
            MIN_SIZE = 7,   // size (4) + ID (2) + checksum (1)
            MAX_SIZE = 255
        };

        enum
        {
            CODE_SUCCESS = 0,
            CODE_USERNAME_ERR = 0xFB,
            CODE_PASSWORD_ERR = 0xFC,
            CODE_UNSUPPORTED = 0xFD,
            CODE_CRC_ERR = 0xFE,
            CODE_FAIL = 0xFF
        };

        static uint16_t in2outID(uint16_t cmdID);

        RcCommand()
        {
            m_buffer.reserve(256);
        }

        bool addPacket(const RcPacket&);

        uint8_t * data() { return &m_buffer[0]; }
        const uint8_t * data() const { return &m_buffer[0]; }

        unsigned size() const { return m_buffer.size(); }
        void resize(unsigned size) { m_buffer.resize(size); }
        void clear() { m_buffer.clear(); }

        unsigned cmdLength() const
        {
            unsigned x = m_buffer[0]; x <<= 8;
            x |= m_buffer[1]; x <<= 8;
            x |= m_buffer[2]; x <<= 8;
            x |= m_buffer[3];
            return x;
        }

        uint16_t commandID() const { return (m_buffer[4]<<8) | m_buffer[5]; }
        uint8_t returnCode() const { return m_buffer[6]; }
        void setReturnCode(uint8_t val) { m_buffer[6] = val; }

        uint8_t calcChecksum() const { return RcPacket::checksum(&m_buffer[0], cmdLength()); }
        uint8_t checksumValue() const { return m_buffer[cmdLength()]; }
        bool checksumOK() const { return validLength() && checksumValue() == calcChecksum(); }

        bool validLength() const { return m_buffer.size() >= MIN_SIZE && m_buffer.size() == cmdLength() + 1; }
        bool isValid() const { return checksumOK(); }

        void mkPackets(SendSequence& sseq, uint16_t rxID, std::vector<RcPacketBuffer>& pkts) const;

    private:
        std::vector<uint8_t> m_buffer;
    };
}

#endif
