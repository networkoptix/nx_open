#ifndef ITE_MPEG_TS_PACKET_H
#define ITE_MPEG_TS_PACKET_H

namespace ite
{
    /// MPEG transport stream packet
    class MpegTsPacket
    {
    public:
        static const uint8_t PACKET_SIZE = 188;
        static const uint8_t SYNC_BYTE = 0x47;

        // predefined PIDs
        enum
        {
            PID_ProgramAssociationTable = 0,
            PID_ConditionalAccessTable = 1,
            PID_TransportStreamDescriptionTable = 2,
            PID_IPMPControlInformationTable = 3,
            // Future use: 4..15
            // DVB metadata: 16..31
            // May be assigned as needed to Program Map Tables: 32..8190
            PID_NullPacket = 0x1FFF
        };

        MpegTsPacket(const uint8_t * data)
        :   data_(data)
        {}

        MpegTsPacket(const char * data)
        :   data_((const uint8_t *)data)
        {}

        uint8_t syncByte() const { return data_[0]; }
        bool syncOK() const { return syncByte() == SYNC_BYTE; }

        bool checkbit() const { return data_[1] & 0x80; }   // Transport Error Indicator
        bool isPES() const { return data_[1] & 0x40; }      // Payload Unit Start Indicator
        bool priority() const { return data_[1] & 0x20; }   // Transport Priority

        uint16_t pid() const
        {
            uint16_t x = (data_[1] & 0x1F);
            x <<= 8;
            x |= data_[2];
            return x;
        }

        uint8_t scrambling() const { return (data_[3] & 0xC0) >> 6; }
        bool hasAdaptation() const { return data_[3] & 0x20; }  // Adaptation field exist
        bool hasPayload() const { return data_[3] & 0x10; }     // Contains payload
        uint8_t counter() const { return data_[3] & 0xF; }      // Continuity counter

        // adaptation

        enum
        {
            ADAPT_AdaptationExtension = 0x01,
            ADAPT_TransportPrivateData = 0x02,
            ADAPT_SplicingPoint = 0x04,
            ADAPT_OPCR = 0x08,
            ADAPT_PCR = 0x10,
            ADAPT_StreamPriority = 0x20,
            ADAPT_RandomAccess = 0x40,
            ADAPT_Discontinuity = 0x80
        };

        uint8_t adaptationLen() const
        {
            if (hasAdaptation())
                return data_[4];
            return 0;
        }

        uint8_t adaptationFlags() const
        {
            if (adaptationLen() > 0)
                return data_[5];
            return 0;
        }

        uint64_t opcr() const
        {
            if (adaptationFlags() && ADAPT_OPCR)
                return *((uint64_t*)&data_[6]) & 0xffffffffffffL;
            return 0;
        }

        uint64_t pcr() const
        {
            if (adaptationFlags() && ADAPT_PCR)
            {
                // 33 + 6 + 9 bits
                // 33 - 90 kHz clock
                // 6 - [reserved]
                // 9 - 27 kHz clock
                uint64_t x = data_[6]; x <<= 8;
                x |= data_[7]; x <<= 8;
                x |= data_[8]; x <<= 8;
                x |= data_[9]; x <<= 1;
                x |= data_[10] & 0x80;

                return x;
            }
            return 0;
        }

        // payload

        const uint8_t * payload() const
        {
            if (! hasPayload())
                return 0;

            if (hasAdaptation())
                return data_ + 5 + adaptationLen();
            return data_ + 4;
        }

        uint8_t payloadLen() const
        {
            if (! hasPayload())
                return 0;

            return PACKET_SIZE - (payload() - data_);
        }

    private:
        const uint8_t * data_;
    };
}

#endif
