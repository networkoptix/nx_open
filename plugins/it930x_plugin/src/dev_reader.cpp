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
        {
            static const unsigned READ_COUNT = 4;

            for (unsigned i = 0; i < READ_COUNT; ++i)
            {
                clear();
                readDevice();
            }
        }
    }

    // Demux

    Demux::Demux()
    {
        // alloc TS pool
        {
            std::vector<TsBuffer> vecTS;
            vecTS.reserve(TsBuffer::MAX_ELEMENTS);

            for (size_t i=0; i< TsBuffer::MAX_ELEMENTS; ++i)
                vecTS.push_back(TsBuffer(m_poolTs));
        }
    }

    void Demux::push(const TsBuffer& ts, unsigned maxCount)
    {
        uint16_t pid = ts.packet().pid();
        TsQueue * q = queue(pid);
        if (!q)
            return;

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (q->size() >= maxCount)
            q->pop_front();

        q->push_back(std::move(ts));
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

    void Demux::pushRC(const TsBuffer& ts)
    {
        push(std::move(ts), MAX_RC_QUEUE_SIZE());
    }

    TsBuffer Demux::popRC()
    {
        TsBuffer ts = pop(It930x::PID_RETURN_CHANNEL);

        //std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        //if (ts)
        //    return ts.copy(); // pool -> default alloc memory
        return ts;
    }

    void Demux::clear(uint16_t pid)
    {
        TsQueue * q = queue(pid);
        if (!q)
            return;

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        q->clear();
    }

    ContentPacketPtr Demux::dumpPacket(uint16_t pid)
    {
        TsQueue * q = queue(pid);
        if (!q)
            return ContentPacketPtr();

        PtsTime& ptsTime = m_times[pid];
        ContentPacketPtr pkt = std::make_shared<ContentPacket>();

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            while (q->size())
            {
                if (pkt->append(q->front().packet()))
                {
                    bool isPES = q->front().packet().isPES();

                    // In fact it's a time of the next PES, cause we dump TS queue then.
                    if (isPES)
                        pkt->setTimestamp( ptsTime.pts2usec(pkt->pts()) );
                }
                else
                {
                    if (pkt->done())
                        break;

                    if (pkt->gotPES())
                    {
#if 0
                        printf("Skip packet (PID: 0x%x)\n", pid);
#endif
                        while (q->size() && ! q->front().packet().isPES())
                            q->pop_front();

                        pkt.reset();
                        break;
                    }
                }

                q->pop_front();
            }
        }

        if (pkt && (pkt->tsErrors() || pkt->seqErrors()))
        {
#if 1
            printf("Skip packet (PID: 0x%x). TS errors: %d; sequence errors: %d\n", pid, pkt->tsErrors(), pkt->seqErrors());
#endif
            pkt.reset();
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
                m_demux.clear(pid);
                return true;

            case It930x::PID_RETURN_CHANNEL:
                m_demux.clear(pid);
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
                m_packetQueues[pid] = PacketsQueue();
                m_demux.clear(pid);
                break;

            case It930x::PID_RETURN_CHANNEL:
                m_demux.clear(pid);
                break;
        }
    }

    void DevReader::start(It930x * dev, bool rcOnly)
    {
        m_buf.setDevice(dev);

        try
        {
            m_readThread = std::thread( DevReadThread(this, rcOnly) );
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
            printf("%s\n", s.c_str());
        }

        m_buf.setDevice(nullptr);
        m_buf.clear(); // flush buffer!
    }

    void DevReader::wait()
    {
        if (m_hasThread)
        {
            m_readThread.join();
            m_hasThread = false;
        }
    }

    bool DevReader::readStep(bool rcOnly)
    {
        TsBuffer ts = m_demux.tsFromPool();
        if (! m_buf.readTSPacket(ts))
            return false;

        MpegTsPacket pkt = ts.packet();
        if (pkt.syncOK())
        {
            if (pkt.pid() == It930x::PID_RETURN_CHANNEL)
            {
                m_demux.pushRC(std::move(ts));
            }
            else if (! rcOnly)
            {
                if (pkt.isPES())
                    dumpPacket(pkt.pid());

                m_demux.push(std::move(ts));
            }

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
            m_cond.notify_all(); // NOTIFY
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
                m_cond.wait_until(lock, system_clock::now() + timeout); // WAIT

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

    bool DevReader::getRetChanCmd(RCCommand& cmd)
    {
        TsBuffer ts = m_demux.popRC();
        if (ts)
        {
            MpegTsPacket pkt = ts.packet();
            cmd.add(pkt.tsPayload(), pkt.tsPayloadLen());
            return true;
        }

        return false;
    }
}
