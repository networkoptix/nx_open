#include "it930x.h"
#include "rc_command.h"
#include "timer.h"

#include "dev_reader.h"
#include "dev_read_thread.h"

namespace ite
{
    INIT_OBJECT_COUNTER(ContentPacket)
    INIT_OBJECT_COUNTER(Demux)
    INIT_OBJECT_COUNTER(DeviceBuffer)
    INIT_OBJECT_COUNTER(DevReader)


    // DeviceBuffer

    bool DeviceBuffer::sync()
    {
        if (synced())
            return true;

        unsigned shift = 0;
        for (unsigned i = 0; i < MpegTsPacket::PACKET_SIZE(); ++i)
        {
            if (synced(i))
            {
                shift = i;
                break;
            }
        }

        if (!shift)
        {
#if 1
            printf("Can't align device reading\n");
#endif
            return false;
        }

#if 1
        printf("Aligning device reading: %d\n", shift);
#endif
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            uint8_t buf[MpegTsPacket::PACKET_SIZE()];

            m_buf.clear();
            m_device->read(buf, MpegTsPacket::PACKET_SIZE() - shift);
        }

        readDevice();
        return synced();
    }

    int DeviceBuffer::read(uint8_t * buf, int bufSize)
    {
        if (bufSize <= size())
        {
            memcpy(buf, data(), bufSize);
            m_pos += bufSize;
            return bufSize;
        }

        int hasRead = size();
        memcpy(buf, data(), size());
        buf += hasRead;
        bufSize -= hasRead;

        int ret = readDevice();
        if (ret < 0)
            return ret;
        if (ret == 0)
            return hasRead;

        return hasRead + read(buf, bufSize);
    }

    int DeviceBuffer::readDevice()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (! m_device || ! m_device->hasStream())
        {
            clear();
            return -1;
        }

        m_buf.resize(m_size);
        int ret = m_device->read( &(*m_buf.begin()), m_buf.size() );
        if (ret < 0) {
            m_buf.clear();
            m_pos = 0;
            return ret;
        }

        if (size_t(ret) != m_size)
            m_buf.resize(ret);
        m_pos = 0;
        return ret;
    }

    void DeviceBuffer::setDevice(It930x * dev)
    {
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            m_device = dev;
        }

        if (dev)
            readDevice();
    }

    // Demux

    void Demux::push(const TsBuffer& ts)
    {
        uint16_t pid = ts.packet().pid();
        TsQueue * q = queue(pid);
        if (!q)
            return;

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (q->size() >= MAX_TS_QUEUE_SIZE())
        {
            q->pop_front();
#if 0
            printf("lost TS\n");
#endif
        }

#if 0
        if (ts.packet().checkbit())
            printf("TS error bit\n");
#endif
#if 0
        if (q->size())
        {
            if ((q->back().packet().counter() + 1) % 16 != ts.packet().counter())
                printf("TS wrong count: %d -> %d\n", q->back().packet().counter(), ts.packet().counter());
        }
#endif
        q->push_back(ts);
    }

    TsBuffer Demux::pop(uint16_t pid)
    {
        TsQueue * q = queue(pid);
        if (!q)
            return TsBuffer();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (q->size())
        {
            TsBuffer ts = q->front();
            q->pop_front();
            return ts;
        }

        return TsBuffer();
    }

    ContentPacketPtr Demux::dumpPacket(uint16_t pid)
    {
        TsQueue * q = queue(pid);
        if (!q)
            return ContentPacketPtr();

        ContentPacketPtr pkt = std::make_shared<ContentPacket>();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        bool gotPES = false;
        while (q->size())
        {
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

    // DevReader

    DevReader::DevReader(It930x * dev)
    :   m_buf(dev),
        m_threadObject(nullptr),
        m_hasThread(false)
    {
        //m_demux.addPid(MpegTsPacket::PID_PAT);
        //m_demux.addPid(MpegTsPacket::PID_CAT);
        m_demux.addPid(It930x::PID_RETURN_CHANNEL);
        m_demux.addPid(Pacidal::PID_VIDEO_FHD);
        m_demux.addPid(Pacidal::PID_VIDEO_HD);
        m_demux.addPid(Pacidal::PID_VIDEO_SD);
        m_demux.addPid(Pacidal::PID_VIDEO_CIF);

        // alloc TS pool
        {
            std::vector<TsBuffer> vecTS;
            vecTS.reserve(TsBuffer::MAX_ELEMENTS);

            for (size_t i=0; i< TsBuffer::MAX_ELEMENTS; ++i)
                vecTS.push_back(TsBuffer(m_poolTs));
        }
    }

    bool DevReader::subscribe(uint16_t pid)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        switch (pid)
        {
            case Pacidal::PID_VIDEO_FHD:
            case Pacidal::PID_VIDEO_HD:
            case Pacidal::PID_VIDEO_SD:
            case Pacidal::PID_VIDEO_CIF:
                m_packetQueues[pid] = PacketsQueue();
                return true;
        }

        return false;
    }

    void DevReader::unsubscribe(uint16_t pid)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        switch (pid)
        {
            case Pacidal::PID_VIDEO_FHD:
            case Pacidal::PID_VIDEO_HD:
            case Pacidal::PID_VIDEO_SD:
            case Pacidal::PID_VIDEO_CIF:
                //m_packetQueues.erase(pid);
                m_packetQueues[pid] = PacketsQueue();
                break;
        }
    }

    void DevReader::start(It930x * dev)
    {
        m_buf.setDevice(dev);

        try
        {
            m_readThread = std::thread( DevReadThread(this) );
            m_hasThread = true;
        }
        catch (std::system_error& )
        {}
    }

    void DevReader::stop()
    {
        try
        {
            if (m_hasThread)
            {
                /// @warning HACK: if m_threadObject creates later join leads to deadlock.
                /// Resource reload mutex could not lock. Camera can't reload itself.
                for (unsigned i = 0; !m_threadObject && i < 100; ++i)
                    usleep(10000);

                if (m_threadObject)
                    m_threadObject->stop();

                m_readThread.join(); // possible deadlock here
                m_hasThread = false;
            }

            m_threadObject = nullptr;
        }
        catch (const std::exception& e)
        {
            std::string s = e.what();
        }

        m_buf.setDevice(nullptr);
    }

    bool DevReader::readStep()
    {
        TsBuffer ts(m_poolTs);
        if (! m_buf.readTSPacket(ts))
            return false;

        MpegTsPacket pkt = ts.packet();
        if (pkt.syncOK())
        {
            if (pkt.isPES())
                dumpPacket(pkt.pid());

            m_demux.push(ts);
            return true;
        }

        return false;
    }

    void DevReader::dumpPacket(uint16_t pid)
    {
        static const unsigned MAX_QUEUE_SIZE = 32; // 30 fps => ~1 sec

        ContentPacketPtr pkt = m_demux.dumpPacket(pid);

        if (pkt && pkt->size())
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            auto it = m_packetQueues.find(pid);
            if (it == m_packetQueues.end())
            {
#if 0
                printf("Packet for unknown PID: %d\n", pid);
#endif
                return;
            }
            PacketsQueue * q = &it->second;

            // TODO: could unlock DevReader (and lock queue) here

            while (q->size() >= MAX_QUEUE_SIZE)
            {
                q->pop_front();
                q->front()->flags() |= ContentPacket::F_StreamReset;
            }

            q->push_back(pkt);
            m_cond.notify_all();
        }
    }

    ContentPacketPtr DevReader::getPacket(uint16_t pid, const std::chrono::milliseconds& timeout)
    {
        using std::chrono::system_clock;

        std::unique_lock<std::mutex> lock( m_mutex ); // LOCK

        auto it = m_packetQueues.find(pid);
        if (it != m_packetQueues.end())
        {
            PacketsQueue& q = it->second;

            if (q.empty())
                m_cond.wait_until(lock, system_clock::now() + timeout);

            // could be unsubscribed here
            if (! q.empty())
            {
                ContentPacketPtr pkt = q.front();
                q.pop_front();
                return pkt;
            }
        }

        return ContentPacketPtr();
    }

    bool DevReader::readRetChanCmd(RCCommand& cmd)
    {
        static const unsigned WAIT_TIME_MS = 1000;
        static const unsigned READ_BLOCK = 1024;

        sync();

        Timer t(true);
        TsBuffer ts(m_poolTs);

        while (t.elapsedUS() < WAIT_TIME_MS)
        {
            // check time time to time - sometimes
            for (unsigned i = 0; i < READ_BLOCK; ++i)
            {
                if (m_buf.readTSPacket(ts))
                {
                    MpegTsPacket pkt = ts.packet();
                    if (pkt.syncOK())
                    {
                        uint16_t pid = pkt.pid();
                        if (pid == It930x::PID_RETURN_CHANNEL)
                        {
#if 1
                            printf("got RC cmd (TX search)\n");
#endif
                            cmd.add(pkt.tsPayload(), pkt.tsPayloadLen());
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    bool DevReader::getRetChanCmd(RCCommand& cmd)
    {
        TsBuffer ts = m_demux.pop(It930x::PID_RETURN_CHANNEL);
        if (ts)
        {
#if 1
            printf("getting RC cmd (from RC queue)\n");
#endif
            MpegTsPacket pkt = ts.packet();
            cmd.add(pkt.tsPayload(), pkt.tsPayloadLen());
            return true;
        }

        return false;
    }
}
