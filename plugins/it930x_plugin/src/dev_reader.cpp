#include "it930x.h"
#include "rc_command.h"

#include "dev_reader.h"
#include "dev_read_thread.h"

namespace ite
{
    DevReader::DevReader(It930x * dev, size_t size)
    :   m_device(dev),
        m_size(size),
        m_pos(0),
        m_threadObject(nullptr),
        m_hasThread(false)
    {
        if (m_size < MIN_PACKETS_COUNT() * MpegTsPacket::PACKET_SIZE())
            m_size = MIN_PACKETS_COUNT() * MpegTsPacket::PACKET_SIZE();

        //m_demux.addPid(MpegTsPacket::PID_PAT);
        //m_demux.addPid(MpegTsPacket::PID_CAT);
        m_demux.addPid(It930x::PID_RETURN_CHANNEL);
        m_demux.addPid(Pacidal::PID_VIDEO_FHD);
        //m_demux.addPid(Pacidal::PID_VIDEO_HD);
        m_demux.addPid(Pacidal::PID_VIDEO_SD);
        //m_demux.addPid(Pacidal::PID_VIDEO_CIF);
    }

    bool DevReader::subscribe(uint16_t pid)
    {
        switch (pid)
        {
            case Pacidal::PID_VIDEO_FHD:
            case Pacidal::PID_VIDEO_HD:
            case Pacidal::PID_VIDEO_SD:
            case Pacidal::PID_VIDEO_CIF:
                return true;
        }

        return false;
    }

    void DevReader::setDevice(It930x * dev)
    {
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (m_device != dev)
                m_device = dev;
        }

        clear();
        if (dev)
            readDevice();
    }

    void DevReader::start(It930x * dev)
    {
        setDevice(dev);

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

        setDevice(nullptr);
    }

    bool DevReader::readStep()
    {
        TsBuffer ts;
        if (! readTSPacket(ts))
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
        static const unsigned MAX_QUEUE_SIZE = 64;

        ContentPacketPtr pkt = m_demux.dumpPacket(pid);
        if (pkt && pkt->size())
        {
            //std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            m_packetQueues[pid].push_back(pkt); // creates queue if not exists
        }

        if (m_packetQueues[pid].size() > MAX_QUEUE_SIZE)
        {
            m_packetQueues[pid].pop_front();
#if 1
            printf("lost packet\n");
#endif
        }
    }

    bool DevReader::readRetChanCmd(RCCommand& cmd)
    {
        if (! synced())
            sync();

        TsBuffer ts;

        static const unsigned MAX_READS = 1000;
        for (unsigned i=0; i < MAX_READS; ++i)
        {
            if (readTSPacket(ts))
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

        return false;
    }

    bool DevReader::getRetChanCmd(RCCommand& cmd)
    {
        TsBuffer ts;
        if (m_demux.pop(It930x::PID_RETURN_CHANNEL, ts))
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

    int DevReader::read(uint8_t * buf, int bufSize)
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

    int DevReader::readDevice()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (! m_device || ! m_device->hasStream())
            return -1;

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
}
