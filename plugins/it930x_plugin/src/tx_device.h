#ifndef RETURN_CHANNEL_DEVICE_INFO_H
#define RETURN_CHANNEL_DEVICE_INFO_H

#include <vector>
#include <string>
#include <mutex>

#include "ret_chan/ret_chan_cmd_host.h"
#include "rc_command.h"


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

    struct TxManufactureInfo
    {
        std::string companyName;
        std::string modelName;
        std::string firmwareVersion;
        std::string serialNumber;
        std::string hardwareId;
    };

    struct TxVideoEncConfig
    {
        uint16_t width;
        uint16_t height;
        uint16_t bitrateLimit;
        uint8_t  frameRateLimit;
        uint8_t  quality;

        TxVideoEncConfig()
        :   width(0),
            height(0),
            bitrateLimit(0),
            frameRateLimit(0),
            quality(0)
        {}
    };

    struct SendInfo
    {
        std::mutex mutex;
        uint16_t txID;
        uint16_t rcSeq;
        uint8_t tsSeq;
        bool tsMode;

        SendInfo(uint16_t tx)
        :   txID(tx),
            rcSeq(0),
            tsSeq(0),
            tsMode(false)
        {}
    };

    ///
    class TxDevice : public TxRC
    {
    public:
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

        TxDevice(unsigned short txID, unsigned freq);
        ~TxDevice();

        SendInfo& sendInfo() { return m_device; }
        uint16_t txID() const { return m_device.txID; }

        unsigned frequency() const { return transmissionParameter.frequency; }
        void setFrequency(unsigned freq) { transmissionParameter.frequency = freq; }

        void print() const;

        // info

        TxManufactureInfo txDeviceInfo() const
        {
            TxManufactureInfo mInfo;
            mInfo.companyName = rcStr2str(manufactureInfo.manufactureName);
            mInfo.modelName = rcStr2str(manufactureInfo.modelName);
            mInfo.firmwareVersion = rcStr2str(manufactureInfo.firmwareVersion);
            mInfo.serialNumber = rcStr2str(manufactureInfo.serialNumber);
            mInfo.hardwareId = rcStr2str(manufactureInfo.hardwareId);
            return mInfo;
        }

        // encoder

        uint8_t encodersCount() const { return videoEncConfig.configListSize; }

        TxVideoEncConfig txVideoEncConfig(uint8_t encoderNo)
        {
            if (encoderNo >= encodersCount())
                return TxVideoEncConfig();

            TxVideoEncConfig conf;
            conf.width = videoEncConfig.configList[encoderNo].width;
            conf.height = videoEncConfig.configList[encoderNo].height;
            conf.bitrateLimit = videoEncConfig.configList[encoderNo].bitrateLimit;
            conf.frameRateLimit = videoEncConfig.configList[encoderNo].frameRateLimit;
            conf.quality = videoEncConfig.configList[encoderNo].quality;
            return conf;
        }

        //bool setVideoEncConfig(unsigned streamNo, const TxVideoEncConfig& conf);

        //

        void ids4update(std::vector<uint16_t>& ids);

        bool parse(RcPacket& pkt);              // RcPacket -> RcCommand -> TxDevice
        RcCommand * mkRcCmd(uint16_t command);  // TxDevice -> RcCommand
        RcCommand * mkSetChannel(unsigned channel);

        // -- RC
        static uint8_t checksum(const uint8_t * buffer, unsigned length) { return RcPacket::checksum(buffer, length); }
        uint8_t * rc_getBuffer(unsigned size);
        unsigned rc_command(Byte * buffer, unsigned bufferSize);
        // --

    private:
        mutable std::mutex m_mutex;
        SendInfo m_device;
        RcCommand m_cmdRecv;
        RcCommand m_cmdSend;

        static std::string rcStr2str(const RCString& s) { return std::string((const char *)s.stringData, s.stringLength); }

        TxDevice(const TxDevice& );
        TxDevice& operator = (const TxDevice& );
    };
}

#endif
