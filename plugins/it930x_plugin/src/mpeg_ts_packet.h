#ifndef ITE_MPEG_TS_PACKET_H
#define ITE_MPEG_TS_PACKET_H

namespace ite
{
    /// MPEG transport stream packet
    class MpegTsPacket
    {
    public:
        static constexpr unsigned PACKET_SIZE() { return 188; }
        static constexpr unsigned SYNC_BYTE() { return 0x47; }
        static constexpr unsigned PES_START_SIZE() { return 6; }
        static constexpr unsigned PES_HEADER_SIZE() { return 9; }
        static constexpr unsigned PES_PTS_SIZE() { return 5; }

        // predefined PIDs
        enum
        {
            PID_PAT = 0,
            PID_CAT = 1,
            PID_TSDT = 2,
            PID_IPMP_CIT = 3,
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
        bool syncOK() const { return syncByte() == SYNC_BYTE(); }

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

        uint64_t adaptOPCR() const
        {
            if (adaptationFlags() && ADAPT_OPCR)
                return *((uint64_t*)&data_[6]) & 0xffffffffffffL;
            return 0;
        }

        uint64_t adaptPCR() const
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

        const uint8_t * tsPayload() const
        {
            if (! hasPayload())
                return nullptr;

            if (hasAdaptation())
                return data_ + 5 + adaptationLen();
            return data_ + 4;
        }

        uint8_t tsPayloadLen() const
        {
            if (! hasPayload())
                return 0;

            return PACKET_SIZE() - (tsPayload() - data_);
        }

        // PES

        typedef enum
        {
            SID_Reserved = 0xbc,        //< program_stream_map ?
            SID_Private1 = 0xbd,
            SID_Padding = 0xbe,
            SID_Private2 = 0xbf,

            SID_MpegAidioMin = 0xc0,    ///< 110x xxxx - MPEG Audio Stream Number xxxxx
            SID_MpegAidioMax = 0xdf,
            SID_MpegVideoMin = 0xe0,    ///< 1110 xxxx - MPEG Video Stream Number xxxx
            SID_MpegVideoMax = 0xef,

            SID_ECM = 0xf0,
            SID_EMM = 0xf1,
            SID_DSM_CC = 0xf2,
            SID_ProgramDirectory = 0xff
        } StreamId;

        ///
        struct PesFlags
        {
            PesFlags(const uint8_t * f = nullptr)
            {
                if (f)
                {
                    flags_[0] = f[0];
                    flags_[1] = f[1];
                }
                else
                    flags_[0] = flags_[1] = 0;
            }

            bool ok() const { return (flags_[0] & 0xc0) == 0x80; }
            uint8_t scrambling() const { return flags_[0] & 0x30; }

            bool priority() const { return flags_[0] & 0x08; }
            bool dataAlignment() const { return flags_[0] & 0x04; }
            bool copyright() const { return flags_[0] & 0x02; }
            bool original() const { return flags_[0] & 0x01; }

            bool hasPTS() const { return flags_[1] & 0x80; }
            bool hasDTS() const { return flags_[1] & 0x40; }
            bool escr() const { return flags_[1] & 0x20; }
            bool esRate() const { return flags_[1] & 0x10; }
            bool dsmTrickMode() const { return flags_[1] & 0x08; }
            bool additionalCopyInfo() const { return flags_[1] & 0x04; }
            bool crc() const { return flags_[1] & 0x02; }
            bool extension() const { return flags_[1] & 0x01; }

        private:
            uint8_t flags_[2];
        };

        bool pesHasPrefix() const
        {
            const uint8_t * pesData = tsPayload();
            if (! pesData || tsPayloadLen() < PES_START_SIZE())
                return false;

            return pesData[0] == 0x00 && pesData[1] == 0x00 && pesData[2] == 0x01;
        }

        uint8_t pesStreamID() const
        {
            const uint8_t * pesData = tsPayload();
            if (! pesData || tsPayloadLen() < PES_START_SIZE())
                return 0;

            return pesData[3];
        }

        /// @note Can be zero. If the PES packet length is set to zero, the PES packet can be of any length.
        uint16_t pesPacketLength() const
        {
            const uint8_t * pesData = tsPayload();
            if (! pesData || tsPayloadLen() < PES_START_SIZE())
                return 0;

            return (((uint16_t)pesData[4])<<8) + pesData[5];
        }

        bool pesHasHeader() const
        {
            if (tsPayloadLen() < PES_HEADER_SIZE())
                return false;

            uint8_t sID = pesStreamID();
            return  (sID != SID_Padding) && (sID != SID_Private2) && (sID != SID_Reserved) &&
                    (sID != SID_ECM) && (sID != SID_EMM) && (sID != SID_DSM_CC) &&
                    (sID != SID_ProgramDirectory);
        }

        const PesFlags pesFlags() const
        {
            if (pesHasHeader())
                return PesFlags(tsPayload() + PES_START_SIZE());
            return PesFlags();
        }

        // gives the length of the remainder of the PES header
        uint8_t pesHeaderLength() const
        {
            const uint8_t * pesData = tsPayload();
            if (! pesData || !pesHasHeader())
                return 0;
            return pesData[8];
        }

        // Parse MPEG-PES five-byte timestamp
        static uint64_t parsePesPTS(const uint8_t * buf)
        {
            int64_t val = buf[0] & 0x0e; val <<= 7;
            val += buf[1]; val <<= 8;
            val += buf[2] & 0xfe; val <<= 7;
            val += buf[3]; val <<= 8;
            val += buf[4] & 0xfe; val >>= 1;
            return val;
        }

        uint64_t pesPTS() const
        {
            if (pesFlags().hasPTS())
                return parsePesPTS(tsPayload() + PES_HEADER_SIZE());
            return 0;
        }

        uint64_t pesDTS() const
        {
            if (pesFlags().hasDTS())
                return parsePesPTS(tsPayload() + PES_HEADER_SIZE() + PES_PTS_SIZE());
            return 0;
        }

        uint32_t pesPTS32() const { return pesPTS() & 0xfffffffful; }
        uint32_t pesDTS32() const { return pesDTS() & 0xfffffffful; }

        // PES payload

        const uint8_t * pesPayload() const
        {
            if (! isPES() || ! pesHasPrefix())
                return nullptr;

            const uint8_t * pload = tsPayload();
            if (pesHasHeader())
                pload += PES_HEADER_SIZE() + pesHeaderLength();
            else
                pload += PES_START_SIZE();

            if (pload >= (data_ + PACKET_SIZE()))
                return nullptr;
            return pload;
        }

        uint8_t pesPayloadLen() const
        {
            const uint8_t * pload = pesPayload();
            if (! pload)
                return 0;

            return (data_ + PACKET_SIZE()) - pload;
        }

        void print()
        {
            printf("TS packet: PID %d; sync %d; errbit %d; PES %d; prio %d; adapt %d; payload %d (%d); count %d\n",
                   pid(), syncOK(), checkbit(), isPES(), priority(), hasAdaptation(), hasPayload(), tsPayloadLen(), counter());

            if (isPES())
            {
                printf("PES: prefix %d; stream %d; header %d; header length %d; PTS %ld; DTS %ld; payload %d\n",
                       pesHasPrefix(), pesStreamID(), pesHasHeader(), pesHeaderLength(), pesPTS(), pesDTS(), pesPayloadLen());
                if (pesHasHeader())
                {
                    const PesFlags f = pesFlags();
                    printf("PES flags: OK %d; scrambling %d; prio %d; alignment %d; copyright %d; original %d; "
                           "PTS %d; DTS %d\n; ESCR %d; ES rate %d; DSM trick mode %d; additional %d; CRC %d; extension %d\n",
                           f.ok(), f.scrambling(), f.priority(), f.dataAlignment(), f.copyright(), f.original(), f.hasPTS(), f.hasDTS(),
                           f.escr(), f.esRate(), f.dsmTrickMode(), f.additionalCopyInfo(), f.crc(), f.extension()
                    );
                }
            }
        }

    private:
        const uint8_t * data_;
    };
}

#endif
