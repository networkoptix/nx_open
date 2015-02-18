#ifndef ITE_DEVREADER_H
#define ITE_DEVREADER_H

#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "object_counter.h"

#include "mpeg_ts_packet.h"

namespace ite
{
    // Pool (avoid memory allocations)
    template<typename T, unsigned MAX_ELEMENTS>
    class Pool
    {
    public:
        Pool()
        {
            m_pool.reserve(MAX_ELEMENTS);
        }

        void push(T elem)
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (m_pool.size() < MAX_ELEMENTS)
            {
                m_pool.push_back(elem);
                elem.reset(); // under lock
            }
            // else do nothing => free
        }

        T pop()
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (m_pool.empty())
                return T();

            T elem = m_pool.back();
            m_pool.pop_back();
            return elem;
        }

    private:
        std::mutex m_mutex;
        std::vector<T> m_pool;
    };

    /// Buffer for MpegTsPacket to place it in std container
    class TsBuffer
    {
    public:
        enum
        {
            MAX_ELEMENTS = 8 * 1024
        };

        typedef std::vector<uint8_t> ContT;
        typedef std::shared_ptr<ContT> BufT;
        typedef Pool<BufT, MAX_ELEMENTS> PoolT;

        TsBuffer()
        :   m_pool(nullptr)
        {
        }

        TsBuffer(PoolT& pool)
        :   m_buf(pool.pop()),
            m_pool(&pool)
        {
            if (!m_buf)
                m_buf = std::make_shared<ContT>(MpegTsPacket::PACKET_SIZE());
        }

        ~TsBuffer()
        {
            if (m_buf.unique())
                m_pool->push(m_buf);
        }

        TsBuffer(const TsBuffer& ts)
        :   m_buf(ts.m_buf),
            m_pool(ts.m_pool)
        {
        }

        TsBuffer& operator = (const TsBuffer& ts)
        {
            if (m_buf && m_buf.unique())
                m_pool->push(m_buf);

            m_buf = ts.m_buf;
            m_pool = ts.m_pool;
            return *this;
        }

        operator bool() const { return m_buf.get(); }

        MpegTsPacket packet() const { return MpegTsPacket(data()); }

        const uint8_t * data() const { return &(*m_buf)[0]; }
        uint8_t * data() { return &(*m_buf)[0]; }

        static unsigned size() { return MpegTsPacket::PACKET_SIZE(); }

    private:
        BufT m_buf;
        PoolT * m_pool;
    };

    typedef std::deque<TsBuffer> TsQueue;

    ///
    class ContentPacket : public ObjectCounter<ContentPacket>
    {
    public:
        static constexpr unsigned MAX_PACKET_SIZE() { return 8 * 1024 * 1024; }

        enum Flags
        {
            F_IsKey         = 0x1,
            F_StreamReset   = 0x2
        };

        ContentPacket()
        :   m_gotPES(0),
            m_flags(0),
            m_tsErrors(0),
            m_pesPTS(0),
            m_pesDTS(0)
            //m_adaptPCR(0),
            //m_adaptOPCR(0),
        {
            static const unsigned SIZE_RESERVE = 16 * 1024;

            m_data.reserve(SIZE_RESERVE);
        }

        void append(const TsBuffer& ts)
        {
            MpegTsPacket pkt = ts.packet();
            if (!m_gotPES && !pkt.isPES())
            {
                //printf("TS lost: waiting for PES\n");
                return;
            }

            if (pkt.isPES())
            {
                ++m_gotPES;
#if 0
                if (pkt.isPES())
                    pkt.print();
#endif
#if 0
                if (m_gotPES > 1)
                    printf("PES already have one!\n");
#endif
                MpegTsPacket::PesFlags flags = pkt.pesFlags();

                if (flags.hasPTS())
                    m_pesPTS = pkt.pesPTS32();
                if (flags.hasDTS())
                    m_pesDTS = pkt.pesDTS32();

                append(pkt.pesPayload(), pkt.pesPayloadLen());
            }
            else
                append(pkt.tsPayload(), pkt.tsPayloadLen());
#if 0
            if (!m_adaptPCR)
                m_adaptPCR = pkt.adaptPCR();
            if (!m_adaptOPCR)
                m_adaptOPCR = pkt.adaptOPCR();
#endif
            if (pkt.checkbit())
                ++m_tsErrors;

            // memory limit
            if (size() >= MAX_PACKET_SIZE())
            {
                printf("Elementary stream packet is too big. Something goes wrong.\n");
                m_gotPES = 0;
                m_data.clear();
            }
        }

        const uint8_t * data() const { return &m_data[0]; }
        size_t size() const { return m_data.size(); }
        bool empty() const { return m_data.empty(); }

        uint32_t pts() const { return m_pesPTS; }
        unsigned& flags() { return m_flags; }

        unsigned errors() const { return m_tsErrors; }

    private:
        std::vector<uint8_t> m_data;
        unsigned m_gotPES;
        unsigned m_flags;
        unsigned m_tsErrors;
        uint32_t m_pesPTS;
        uint32_t m_pesDTS;
#if 0
        uint64_t m_adaptPCR;
        uint64_t m_adaptOPCR;
#endif

        void append(const uint8_t * apData, uint8_t dataSize)
        {
            if (apData && dataSize)
            {
                size_t oldSize = m_data.size();
                m_data.resize(oldSize + dataSize);

                memcpy(&m_data[oldSize], apData, dataSize);
            }
#if 0
            else
                printf("TS empty: data %p; len %d\n", apData, dataSize);
#endif
        }
    };

    typedef std::shared_ptr<ContentPacket> ContentPacketPtr;

    /// Splits transport stream by PID
    class Demux : public ObjectCounter<Demux>
    {
    public:
        static constexpr unsigned MAX_TS_QUEUE_SIZE() { return TsBuffer::MAX_ELEMENTS; }

        void addPid(uint16_t pid)
        {
            m_queues[pid] = TsQueue();
        }

        void push(const TsBuffer& ts);
        TsBuffer pop(uint16_t pid);

        ContentPacketPtr dumpPacket(uint16_t pid);

    private:
        mutable std::mutex m_mutex;
        std::map<uint16_t, TsQueue> m_queues;   // {PID, TS queue}

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

    class It930x;

    ///
    class DeviceBuffer : public ObjectCounter<DeviceBuffer>
    {
    public:
        static constexpr unsigned MIN_PACKETS_COUNT() { return 5; }
        static constexpr unsigned DEFAULT_PACKETS_NUM() { return 816; }

        DeviceBuffer(It930x * dev = nullptr, size_t size = DEFAULT_PACKETS_NUM() * MpegTsPacket::PACKET_SIZE())
        :   m_device(dev),
            m_size(size),
            m_pos(0)
        {
            if (m_size < MIN_PACKETS_COUNT() * MpegTsPacket::PACKET_SIZE())
                m_size = MIN_PACKETS_COUNT() * MpegTsPacket::PACKET_SIZE();
        }

        void setDevice(It930x * dev);

        bool synced(uint8_t shift = 0) const
        {
            if (m_buf.size() < MIN_PACKETS_COUNT() * MpegTsPacket::PACKET_SIZE())
                return false;

            if (MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 0).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 1).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 2).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE() * 3).syncOK())
                return true;

            return false;
        }

        /// @note sync() and read() are not thread safe. m_pos changes are not under mutex.
        /// Do not want to lock every read(). Call sync() and read() in the same thread!

        bool sync();

        bool readTSPacket(TsBuffer& buf)
        {
            return read(buf.data(), buf.size()) == (int)buf.size();
        }

    private:
        mutable std::mutex m_mutex;

        It930x * m_device;
        std::vector<uint8_t> m_buf;
        size_t m_size;
        size_t m_pos;

        const uint8_t * data() const { return &m_buf[m_pos]; }
        int size() const { return m_buf.size() - m_pos; }

        void clear()
        {
            m_buf.clear();
            m_pos = 0;
        }

        int read(uint8_t * buf, int bufSize);
        int readDevice();
    };

    class DevReadThread;
    class RCCommand;

    ///
    class DevReader : public ObjectCounter<DevReader>
    {
    public:
        DevReader(It930x * dev = nullptr);

        bool subscribe(uint16_t pid);
        void unsubscribe(uint16_t pid);

        void setDevice(It930x * dev) { m_buf.setDevice(dev); }
        void start(It930x * dev);
        void stop();

        void setThreadObj(DevReadThread * ptr) { m_threadObject = ptr; }
        bool hasThread() const { return m_threadObject; }

        bool sync()
        {
            if (! m_buf.synced())
                return m_buf.sync();
            return true;
        }

        bool readStep();
        bool readRetChanCmd(RCCommand& cmd);
        bool getRetChanCmd(RCCommand& cmd);

        ContentPacketPtr getPacket(uint16_t pid, const std::chrono::milliseconds& timeout);
        void interrupt() const { m_cond.notify_all(); }

    private:
        typedef std::deque<ContentPacketPtr> PacketsQueue;

        mutable std::mutex m_mutex;
        mutable std::condition_variable m_cond;

        DeviceBuffer m_buf;
        Demux m_demux;
        TsBuffer::PoolT m_poolTs;
        std::map<uint16_t, PacketsQueue> m_packetQueues;

        DevReadThread * m_threadObject;
        std::thread m_readThread;
        bool m_hasThread;

        void dumpPacket(uint16_t pid);
    };
}

#endif // ITE_DEVREADER_H
