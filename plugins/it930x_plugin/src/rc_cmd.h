#ifndef RETURN_CHANNEL_CMD_X_H
#define RETURN_CHANNEL_CMD_X_H

#include <vector>

#include "ret_chan/ret_chan_cmd.h"
#include "ret_chan/ret_chan_cmd_host.h"
#include "ret_chan/ret_chan_user.h"
#include "ret_chan/ret_chan_user_host.h"

namespace ite
{
    ///
    class Command
    {
    public:
        Command()
        {
            buffer_.reserve(RcCmdMaxSize);
        }

        unsigned add(Byte * data, unsigned len)
        {
            for (unsigned i = 0; buffer_.size() < RcCmdMaxSize && len > 0; ++i, --len)
                buffer_.push_back(data[i]);
            return len;
        }

        bool isFull() const { return buffer_.size() == RcCmdMaxSize; }
        bool hasLeadingTag() const { return buffer_[0] == leadingTag(); }
        bool hasEndingTag() const { return buffer_[183] == endingTag(); }
        bool hasTags() const { return hasLeadingTag() && hasEndingTag(); }
        bool isOK() const { return isFull() && hasTags() && checksum(); }

        void clear() { buffer_.clear(); }
        void swap(Command& cmd) { buffer_.swap(cmd.buffer_); }

        static unsigned short headSize() { return 13; }
        static Byte leadingTag() { return '#'; }
        static Byte endingTag() { return 0x0d; }
        static unsigned short maxSize() { return RcCmdMaxSize; }

        unsigned short rxID() const { return ((buffer_[1])<<8) | buffer_[2]; }
        unsigned short txID() const { return (buffer_[3]<<8) | buffer_[4]; }
        unsigned short commandID() const { return (buffer_[17]<<8) | buffer_[18]; }
        unsigned short totalPktNum() const { return (buffer_[5]<<8) | buffer_[6]; }
        unsigned short paketNum() const { return (buffer_[7]<<8) | buffer_[8]; }
        unsigned short sequenceNum() const { return (buffer_[9]<<8) | buffer_[10]; }
        unsigned short pktLength() const { return (buffer_[11]<<8) | buffer_[12]; }

        Byte checkSumValue() const { return buffer_[headSize() + pktLength()]; }
        bool checksum() const;

        bool isBroadcast() const { return txID() == 0xFFFF; }

        const Byte * head() const { return &buffer_[0]; }
        const Byte * data() const { return &buffer_[headSize()]; }

        unsigned headCheck() const;

        bool checkPacketCount();

    private:
        std::vector<Byte> buffer_;
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

        unsigned add(const Command&);

        const Byte * data() const { return &cmd_[0]; }
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

        unsigned short commandID() const { return (cmd_[4]<<8) | cmd_[5]; }
        Byte returnCode() const { return cmd_[6]; }

    private:
        std::vector<Byte> cmd_;
        unsigned expectedSize_;
        unsigned expectedPackets_;
        unsigned packets_;
        bool isValid_;
    };
}

#endif
