#ifndef RETURN_CHANNEL_DEVICE_INFO_H
#define RETURN_CHANNEL_DEVICE_INFO_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

#include "ret_chan/ret_chan.h"
#include "rc_command.h"
#include "timer.h"
#include "object_counter.h"

namespace nxcip
{
    struct LiveStreamConfig;
}

namespace ite
{
    typedef enum
    {
        RCERR_NO_ERROR = 0,
        RCERR_NO_DEVICE,
        RCERR_SYNTAX,
        RCERR_CHECKSUM,
        RCERR_SEQUENCE,
        RCERR_WRONG_LENGTH,
        RCERR_RET_CODE,
        RCERR_NOT_PARSED,
        RCERR_WRONG_CMD,
        RCERR_METADATA,
        RCERR_OTHER
    } RCError;

    ///
    struct SendSequence
    {
        std::mutex mutex;
        uint16_t txID;
        uint16_t rcSeq;
        uint8_t tsSeq;
        bool tsMode;

        SendSequence(uint16_t tx)
        :   txID(tx),
            rcSeq(0),
            tsSeq(0),
            tsMode(false)
        {}
    };

    ///
    struct TxChannels
    {
        enum
        {
            CHANNELS_NUM = 16
        };

        typedef enum
        {
            FREQ_X = 150000,
            FREQ_CH00 = 177000,
            FREQ_CH01 = 189000,
            FREQ_CH02 = 201000,
            FREQ_CH03 = 213000,
            FREQ_CH04 = 225000,
            FREQ_CH05 = 237000,
            FREQ_CH06 = 249000,
            FREQ_CH07 = 261000,
            FREQ_CH08 = 273000,
            FREQ_CH09 = 363000,
            FREQ_CH10 = 375000,
            FREQ_CH11 = 387000,
            FREQ_CH12 = 399000,
            FREQ_CH13 = 411000,
            FREQ_CH14 = 423000,
            FREQ_CH15 = 473000
        } Frequency;

        static Frequency freq4chan(unsigned channel)
        {
            switch (channel)
            {
                case 0: return FREQ_CH00;
                case 1: return FREQ_CH01;
                case 2: return FREQ_CH02;
                case 3: return FREQ_CH03;
                case 4: return FREQ_CH04;
                case 5: return FREQ_CH05;
                case 6: return FREQ_CH06;
                case 7: return FREQ_CH07;
                case 8: return FREQ_CH08;
                case 9: return FREQ_CH09;
                case 10: return FREQ_CH10;
                case 11: return FREQ_CH11;
                case 12: return FREQ_CH12;
                case 13: return FREQ_CH13;
                case 14: return FREQ_CH14;
                case 15: return FREQ_CH15;
                default:
                    break;
            }
            return FREQ_X;
        }

        static unsigned chan4freq(unsigned freq)
        {
            switch (freq)
            {
                case FREQ_CH00: return 0;
                case FREQ_CH01: return 1;
                case FREQ_CH02: return 2;
                case FREQ_CH03: return 3;
                case FREQ_CH04: return 4;
                case FREQ_CH05: return 5;
                case FREQ_CH06: return 6;
                case FREQ_CH07: return 7;
                case FREQ_CH08: return 8;
                case FREQ_CH09: return 9;
                case FREQ_CH10: return 10;
                case FREQ_CH11: return 11;
                case FREQ_CH12: return 12;
                case FREQ_CH13: return 13;
                case FREQ_CH14: return 14;
                case FREQ_CH15: return 15;
                default:
                    break;
            }
            return 0xffff;
        }
    };

    ///
    class TxDevice : public TxRC, public TxChannels
    {
    public:
        TxDevice(unsigned short txID, unsigned freq);

        SendSequence& sendSequence() { return m_sequence; }
        uint16_t txID() const { return m_sequence.txID; }

        unsigned frequency() const { return transmissionParameter.frequency; }
        void setFrequency(unsigned freq) { transmissionParameter.frequency = freq; }

        void open()
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            resetWanted();
            m_responses.clear();
            m_ready.store(false);
        }

        void close()
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            resetWanted();
            m_ready.store(false);
        }

        bool ready() const { return m_ready; }
        void setReady() { m_ready.store(true); }

        //

        uint8_t encodersCount() const { return videoEncConfig.configListSize; }
        bool videoSourceCfg(unsigned stream, int& width, int& height, float& fps);
        bool videoEncoderCfg(unsigned encNo, nxcip::LiveStreamConfig& config);

        //

        uint16_t cmd2update();
        bool setWanted(uint16_t cmd);
        uint16_t wantedCmd() { return m_wantedCmd; }

        bool responseCode(uint16_t cmd, uint8_t& outCode);
        void clearResponse(uint16_t cmd);
        unsigned responsesCount() const;

        void parse(RcPacket& pkt);              // recv: DevReader -> Demux -> (some thread) -> RcPacket -> RcCommand -> TxDevice
        RcCommand * mkRcCmd(uint16_t command);  // send: RxDevice -> TxDevice -> RcCommand -> RxDevice -> It930x
        RcCommand * mkSetChannel(unsigned channel);
        RcCommand * mkSetEncoderParams(unsigned streamNo, unsigned fps, unsigned bitrateKbps);
        RcCommand * mkSetOSD(unsigned osdNo = 0);

        // -- RC
        static uint8_t checksum(const uint8_t * buffer, unsigned length) { return RcPacket::checksum(buffer, length); }
        uint8_t * rc_getBuffer(unsigned size);
        unsigned rc_command(Byte * buffer, unsigned bufferSize);
        Security& rc_security();
        bool rc_checkSecurity(Security& s);
        // --

        static std::string rcStr2str(const RCString& s)
        {
            return std::string((const char *)s.data(), s.length());
        }

        static void str2rcStr(const std::string& str, RCString& rcStr)
        {
            rcStr.set((const unsigned char *)str.c_str(), str.size());
        }

    private:
        mutable std::mutex m_mutex;
        mutable std::atomic_bool m_ready;
        std::atomic_ushort m_wantedCmd;
        SendSequence m_sequence;
        RcCommand m_cmdRecv;
        RcCommand m_cmdSend;
        Security m_recvSecurity;
        std::map<uint16_t, uint8_t> m_responses; // {cmdID, RetCode}
        Timer m_timer;

        void resetWanted(uint16_t cmd = 0, uint8_t code = 0);

        bool hasResponses(const uint16_t * cmdInputIDs, unsigned size) const;
        bool hasResponse(uint16_t cmdInputID) const { return hasResponses(&cmdInputID, 1); }
        bool supported(uint16_t cmdInputID) const;

        bool prepareTransmissionParams();
        bool prepareVideoEncoderParams(unsigned streamNo);
        bool prepareDateTime();
        bool prepareOSD(unsigned videoSource = 0);

        TxDevice(const TxDevice& );
        TxDevice& operator = (const TxDevice& );
    };

    ///
    struct TxManufactureInfo
    {
        TxManufactureInfo(TxRC& tx)
        :   m_tx(tx)
        {}

        std::string companyName() { return TxDevice::rcStr2str(m_tx.manufactureInfo.manufactureName); }
        std::string modelName() { return TxDevice::rcStr2str(m_tx.manufactureInfo.modelName); }
        std::string firmwareVersion() { return TxDevice::rcStr2str(m_tx.manufactureInfo.firmwareVersion); }
        std::string serialNumber() { return TxDevice::rcStr2str(m_tx.manufactureInfo.serialNumber); }
        std::string hardwareId() { return TxDevice::rcStr2str(m_tx.manufactureInfo.hardwareId); }

    private:
        const TxRC& m_tx;
    };
}

#endif
