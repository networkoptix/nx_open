#ifndef ITE_DEVREADER_H
#define ITE_DEVREADER_H

#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

#include "mpeg_ts_packet.h"

namespace ite
{
    /// Buffer for MpegTsPacket to place it in std container
    class TsBuffer
    {
    public:
        typedef std::vector<uint8_t> ContT;
        typedef std::shared_ptr<ContT> BufT;

        TsBuffer()
        :   m_buf(std::make_shared<ContT>(MpegTsPacket::PACKET_SIZE()))
        {}

        TsBuffer(const TsBuffer& p)
        :   m_buf(p.m_buf)
        {}

        MpegTsPacket packet() const { return MpegTsPacket(data()); }

        const uint8_t * data() const { return &(*m_buf)[0]; }
        uint8_t * data() { return &(*m_buf)[0]; }

        static unsigned size() { return MpegTsPacket::PACKET_SIZE(); }

    private:
        BufT m_buf;
    };

    typedef std::deque<TsBuffer> TsQueue;

    ///
    class ContentPacket
    {
    public:
        ContentPacket()
        :   m_gotPES(0),
            m_pesPTS(0),
            m_pesDTS(0),
            m_adaptPCR(0),
            m_adaptOPCR(0)
        {
            static const unsigned SIZE_RESERVE = 16 * 1024;

            m_data.reserve(SIZE_RESERVE);
        }

        void append(const TsBuffer& buf)
        {
            MpegTsPacket pkt = buf.packet();
            if (!m_gotPES && !pkt.isPES())
                return;

            if (pkt.isPES())
            {
                ++m_gotPES;
#if 0
                if (pkt.isPES())
                    pkt.print();
#endif
#if 1
                if (m_gotPES > 1)
                    printf("PES already have one!\n");
#endif
                MpegTsPacket::PesFlags flags = pkt.pesFlags();

                if (flags.hasPTS())
                    m_pesPTS = pkt.pesPTS();
                if (flags.hasDTS())
                    m_pesDTS = pkt.pesDTS();

                append(pkt.pesPayload(), pkt.pesPayloadLen());
            }
            else
                append(pkt.tsPayload(), pkt.tsPayloadLen());

            if (!m_adaptPCR)
                m_adaptPCR = pkt.adaptPCR();
            if (!m_adaptOPCR)
                m_adaptOPCR = pkt.adaptOPCR();
        }

        const uint8_t * data() const { return &m_data[0]; }
        size_t size() const { return m_data.size(); }
        bool empty() const { return m_data.empty(); }

        int64_t pts() const { return m_pesPTS; }

    private:
        std::vector<uint8_t> m_data;
        unsigned m_gotPES;
        int64_t m_pesPTS;
        int64_t m_pesDTS;
        uint64_t m_adaptPCR;
        uint64_t m_adaptOPCR;

        void append(const uint8_t * apData, uint8_t dataSize)
        {
            if (apData && dataSize)
            {
                size_t oldSize = m_data.size();
                m_data.resize(oldSize + dataSize);

                memcpy(&m_data[oldSize], apData, dataSize);
            }
#if 1
            else
                printf("empty TS: data %p; len %d\n", apData, dataSize);
#endif
        }
    };

    typedef std::shared_ptr<ContentPacket> ContentPacketPtr;

    /// Splits transport stream by PID
    class Demux
    {
    public:
        static constexpr unsigned MAX_TS_QUEUE_SIZE() { return 1024; }

        void addPid(uint16_t pid)
        {
            m_queues[pid] = TsQueue();
        }

        void push(TsBuffer& ts)
        {
            uint16_t pid = ts.packet().pid();
            TsQueue * q = queue(pid);
            if (!q)
                return;

            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (q->size() >= MAX_TS_QUEUE_SIZE())
            {
                q->pop_front(); // TODO: save TsBuffer for future use
#if 1
                printf("lost TS\n");
#endif
            }

            q->push_back(ts);
        }

        void pushFront(TsBuffer& ts)
        {
            uint16_t pid = ts.packet().pid();
            TsQueue * q = queue(pid);
            if (!q)
                return;

            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            q->push_front(ts);
        }

        bool pop(uint16_t pid, TsBuffer& ts)
        {
            TsQueue * q = queue(pid);
            if (!q)
                return false;

            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (q->size())
            {
                ts = q->front(); // TODO: save TsBuffer for future use
                q->pop_front();
                return true;
            }

            return false;
        }

        size_t size(uint16_t pid) const
        {
            const TsQueue * q = queue(pid);
            if (!q)
                return 0;

            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            return q->size();
        }

        ContentPacketPtr dumpPacket(uint16_t pid)
        {
            TsQueue * q = queue(pid);
            if (!q)
                return 0;

            ContentPacketPtr pkt = std::make_shared<ContentPacket>();

            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            bool gotPES = false;
            while (q->size())
            {
                TsBuffer ts = q->front();

                if (q->front().packet().isPES())
                {
                    if (gotPES)
                        break;
                    gotPES = true;
                }

                pkt->append(q->front());
                q->pop_front();
            }

            return pkt;
        }

    private:
        mutable std::mutex m_mutex;
        std::map<uint16_t, TsQueue> m_queues;       // {PID, TS queue}

        TsQueue * queue(uint16_t pid)
        {
            auto it = m_queues.find(pid);
            if (it != m_queues.end())
                return &it->second;
            return nullptr;
        }

        const TsQueue * queue(uint16_t pid) const
        {
            auto it = m_queues.find(pid);
            if (it != m_queues.end())
                return &it->second;
            return nullptr;
        }
    };


    class DevReadThread;
    class It930x;
    class RCCommand;

    ///
    class DevReader
    {
    public:
        static constexpr unsigned MIN_PACKETS_COUNT() { return 5; }
        static constexpr unsigned DEFAULT_PACKETS_NUM() { return 816; }

        DevReader(It930x * dev = nullptr, size_t size = DEFAULT_PACKETS_NUM() * MpegTsPacket::PACKET_SIZE());

        bool subscribe(uint16_t pid);
        void unsubscribe(uint16_t /*pid*/) {}

        void setDevice(It930x * dev);
        void start(It930x * dev);
        void stop();

        void setThreadObj(DevReadThread * ptr) { m_threadObject = ptr; }
        bool hasThread() const { return m_threadObject; }

        bool synced() const
        {
            if (m_buf.size() < MIN_PACKETS_COUNT() * MpegTsPacket::PACKET_SIZE())
                return false;

            uint8_t shift = m_pos % MpegTsPacket::PACKET_SIZE();
            if (MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 0).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 1).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 2).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 3).syncOK())
                return true;

            return false;
        }

        bool sync()
        {
            static const unsigned SYNC_COUNT = 2;

            for (unsigned i = 0; i < SYNC_COUNT * MpegTsPacket::PACKET_SIZE(); ++i)
            {
                if (synced())
                    return true;
                uint8_t val;
                read(&val, 1);
            }
            return false;
        }

        bool readStep();
        bool readRetChanCmd(RCCommand& cmd);
        bool getRetChanCmd(RCCommand& cmd);

        ContentPacketPtr getPacket(uint16_t pid)
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            // FIXME
            auto it = m_packetQueues.find(pid);
            if (it != m_packetQueues.end())
            {
                if (it->second.size())
                {
                    PacketsQueue& q = it->second;
                    ContentPacketPtr pkt = q.front();
                    it->second.pop_front();
                    return pkt;
                }
            }

            return ContentPacketPtr();
        }

    private:
        typedef std::deque<ContentPacketPtr> PacketsQueue;

        mutable std::mutex m_mutex;

        It930x * m_device;
        std::vector<uint8_t> m_buf;
        size_t m_size;
        size_t m_pos;

        Demux m_demux;
        std::map<uint16_t, PacketsQueue> m_packetQueues;

        DevReadThread * m_threadObject;
        std::thread m_readThread;
        bool m_hasThread;

        const uint8_t * data() const { return &m_buf[m_pos]; }
        int size() const { return m_buf.size() - m_pos; }

        void clear()
        {
            m_buf.clear();
            m_pos = 0;
        }

        int read(uint8_t * buf, int bufSize);
        int readDevice();

        bool readTSPacket(TsBuffer& buf)
        {
            return read(buf.data(), buf.size()) == (int)buf.size();
        }

        void dumpPacket(uint16_t pid);
    };
}

#endif // ITE_DEVREADER_H
