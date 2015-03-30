#include <sstream>
#include <thread>

#include "timer.h"
#include "tx_device.h"
#include "mpeg_ts_packet.h"
#include "rx_device.h"

namespace ite
{
    std::mutex It930x::m_rcMutex;

    unsigned str2num(std::string& s)
    {
        unsigned value;
        std::stringstream ss;
        ss << s;
        ss >> value;
        return value;
    }

    std::string num2str(unsigned id)
    {
        std::stringstream ss;
        ss << id;
        return ss.str();
    }

    //

    RxDevice::RxDevice(unsigned id)
    :   m_devReader( new DevReader() ),
        m_rxID(id),
        m_passive(false),
        m_channel(NOT_A_CHANNEL),
        m_txs(TxDevice::CHANNELS_NUM),
        m_signalQuality(0),
        m_signalStrength(0),
        m_signalPresent(false)
    {
        open();
    }

    bool RxDevice::open()
    {
        try
        {
            m_device.reset(); // dtor first
            m_device.reset(new It930x(m_rxID));
            m_device->info(m_rxInfo);
            return true;
        }
        catch (const char * msg)
        {
            m_devReader->stop();
            m_device.reset();
        }

        return false;
    }

    bool RxDevice::wantedByCamera() const
    {
        {
            std::unique_lock<std::mutex> cvLock( m_sync.mutex ); // LOCK

            if (m_sync.waiting)
                return true;
        }

        if (m_sync.usedByCamera)
            return true;

        return false;
    }

    bool RxDevice::lockCamera(TxDevicePtr txDev)
    {
        using std::chrono::system_clock;

        static const unsigned TIMEOUT_MS = 5000;
        static const std::chrono::milliseconds timeout(TIMEOUT_MS);

        if (! txDev)
            return false;

        uint16_t txID = txDev->txID();
        unsigned chan = chan4Tx(txID);

        // lock only one camera
        bool expected = false;
        if (m_sync.usedByCamera.compare_exchange_strong(expected, true))
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                if (tryLockC(chan))
                {
                    if (good())
                        m_devReader->start(m_device.get());

                    m_txDev = txDev;
                    m_txDev->open();
                    return true;
                }

                if (i)
                {
                    m_sync.usedByCamera = false;
                    break;
                }

                std::unique_lock<std::mutex> cvLock( m_sync.mutex ); // LOCK

                m_sync.waiting = true;
                m_sync.cond.wait_until(cvLock, system_clock::now() + timeout); // WAIT

                // usedByCamera is false here (unset on channel unlock)

                m_sync.usedByCamera = true;
                m_sync.waiting = false;
            }
        }

        debug_printf("-- Can't lock channel for camera Tx: %d; Rx: %d (used: %d)\n",
                     txID, m_rxID, m_sync.usedByCamera.load());
        return false;
    }

    bool RxDevice::tryLockC(unsigned channel, bool prio)
    {
        if (channel >= RxDevice::NOT_A_CHANNEL)
            return false;

        if (! prio && wantedByCamera())
            return false;

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (! prio && wantedByCamera())
            return false;

        // prevent double locking at all (even with the same frequency)
        if (isLocked_u())
            return false;

        if (!m_device && !open())
            return false;

        try
        {
            //static const unsigned QUALITY_TIMEOUT_MS = 1000;

            m_device->lockFrequency( TxDevice::freq4chan(channel) );
            m_channel = channel;

            //Timer::sleep(QUALITY_TIMEOUT_MS);
            stats();

            //if (good())
            //    m_devReader->setDevice(m_device.get());
            return true;
        }
        catch (const char * msg)
        {
            m_devReader->stop();
            m_device.reset();
        }

        return false;
    }

    void RxDevice::unlockC()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (m_devReader)
            m_devReader->stop();

        if (m_device)
            m_device->unlockFrequency();

        if (m_txDev)
            m_txDev->close();
        m_txDev.reset();
        m_channel = NOT_A_CHANNEL;

        m_sync.usedByCamera = false;
        m_sync.cond.notify_all(); // NOTIFY
    }

    bool RxDevice::isLocked() const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        return isLocked_u();
    }

    bool RxDevice::stats()
    {
        if (!m_device)
            return false;

        try
        {
            m_signalQuality = 0;
            m_signalStrength = 0;
            m_signalPresent = false;
            bool locked = false;
            m_device->statistic(m_signalQuality, m_signalStrength, m_signalPresent, locked);
            return true;
        }
        catch (const char * msg)
        {
            m_devReader->stop();
            m_device.reset();
        }

        return false;
    }

    void RxDevice::startSearchLost(unsigned lostNum)
    {
        unsigned num = 0;
        for (size_t chan = 0; chan < m_txs.size(); ++chan)
        {
            if (m_txs[chan].lost)
            {
                if (num == lostNum)
                {
                    startSearchTx(chan);
                    return;
                }

                ++num;
            }
        }
    }

    void RxDevice::startSearchTx(unsigned channel)
    {
        if (tryLockC(channel, false))
        {
            debug_printf("searching Tx. Rx: %d; frequency: %d; quality: %d; strength: %d; presence: %d\n",
                   rxID(), TxDevice::freq4chan(channel), quality(), strength(), present());

            if (good())
            {
                m_devReader->subscribe(It930x::PID_RETURN_CHANNEL);
                m_devReader->start(m_device.get(), true);
            }

            m_sync.searching.store(true);
        }
        else
            debug_printf("can't search, used. Rx: %d; frequency: %d\n", rxID(), TxDevice::freq4chan(channel));
    }

    void RxDevice::stopSearchTx(DevLink& devLink)
    {
        devLink.rxID = m_rxID;
        devLink.txID = 0;

        if (m_sync.searching && isLocked())
        {
            if (good())
            {
                m_devReader->wait();

                bool first = true;
                for (;;)
                {
                    RcPacketBuffer pktBuf;
                    if (! m_devReader->getRcPacket(pktBuf))
                        break;

                    RcPacket pkt = pktBuf.packet();
                    if (! pkt.isOK())
                    {
                        debug_printf("[RC] bad packet\n");
                        continue;
                    }

                    if (first)
                    {
                        first = false;
                        devLink.txID = pkt.txID();
                        devLink.channel = m_channel;

                        setTx(m_channel, devLink.txID);
                    }

                    if (pkt.txID() != devLink.txID)
                        debug_printf("-- Different TxIDs from one channel: %d vs %d\n", devLink.txID, pkt.txID());
                    if (! devLink.txID)
                        debug_printf("txID == 0\n");
                }

                m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);
            }

            unlockC();
        }

        m_sync.searching.store(false);
    }

    //

    void RxDevice::processRcQueue()
    {
        if (! m_txDev)
            return;

        RcPacketBuffer pktBuf;
        while (m_devReader->getRcPacket(pktBuf))
        {
            RcPacket pkt = pktBuf.packet();
            if (! pkt.isOK())
            {
                debug_printf("[RC] bad packet\n");
                continue;
            }

            m_txDev->parse(pkt);
        }
    }

    void RxDevice::updateTxParams()
    {
        if (! m_txDev)
            return;

        uint16_t id = m_txDev->cmd2update();
        if (id)
        {
            RcCommand * pcmd = m_txDev->mkRcCmd(id);
            if (pcmd && pcmd->isValid())
                sendRC(pcmd);
        }
    }

    bool RxDevice::sendRC(RcCommand * cmd)
    {
        TxDevicePtr txDev = m_txDev;

        bool ok = cmd && cmd->isValid() && txDev && m_device;
        if (!ok)
            return false;

        uint16_t cmdID = cmd->commandID();

        if (! txDev->setWanted(cmdID))
            return false; // can't send, waiting for response

        SendSequence& sseq = txDev->sendSequence();

        std::lock_guard<std::mutex> lock(sseq.mutex); // LOCK

        std::vector<RcPacketBuffer> pkts;
        cmd->mkPackets(sseq, m_rxID, pkts);

        /// @warning possible troubles with 1-port devices if multipacket
        for (auto itPkt = pkts.begin(); itPkt != pkts.end(); ++itPkt)
            m_device->sendRcPacket(itPkt->packet());

        return true;
    }

    //

    bool RxDevice::changeChannel(unsigned chan)
    {
        if (m_txDev)
        {
            RcCommand * pcmd = m_txDev->mkSetChannel(chan);
            return sendRC(pcmd);
        }

        return false;
    }

    bool RxDevice::setEncoderParams(unsigned streamNo, unsigned fps, unsigned bitrateKbps)
    {
        static const unsigned ATTEMPTS_COUNT = 10;
        static const unsigned DELAY_MS = 200;

        if (m_txDev)
        {
            RcCommand * pcmd = m_txDev->mkSetEncoderParams(streamNo, fps, bitrateKbps);

            for (unsigned i = 0; i < ATTEMPTS_COUNT; ++i)
            {
                if (sendRC(pcmd))
                    return true;
                Timer::sleep(DELAY_MS);
            }

            return false;
        }

        return false;
    }

    //

    void RxDevice::resolutionPassive(unsigned stream, int& width, int& height, float& fps)
    {
        uint16_t pid = stream2pid(stream);

        fps = 30.0f;
        switch (pid)
        {
        case Pacidal::PID_VIDEO_FHD:
            width = 1920;
            height = 1080;
            break;

        case Pacidal::PID_VIDEO_HD:
            width = 1280;
            height = 720;
            break;

        case Pacidal::PID_VIDEO_SD:
            width = 640;
            height = 360;
            break;

        case Pacidal::PID_VIDEO_CIF:
            width = 0;
            height = 0;
            break;
        }
    }

    void RxDevice::resolution(unsigned stream, int& width, int& height, float& fps)
    {
        if (m_passive)
        {
            resolutionPassive(stream, width, height, fps);
            return;
        }

        if (! m_txDev)
        {
            resolutionPassive(stream, width, height, fps);
            return;
        }

        if (! m_txDev->videoSourceCfg(stream, width, height, fps))
            resolutionPassive(stream, width, height, fps);
    }

    unsigned RxDevice::streamsCount()
    {
        if (m_passive)
            return streamsCountPassive();

        if (m_txDev)
            return m_txDev->encodersCount();
        return 0;
    }

    //

    unsigned RxDevice::dev2id(const std::string& nm)
    {
        std::string name = nm;
        name.erase( name.find( DEVICE_PATTERN ), strlen( DEVICE_PATTERN ) );

        return str2id(name);
    }

    unsigned RxDevice::str2id(const std::string& name)
    {
        unsigned num;
        std::stringstream ss;
        ss << name;
        ss >> num;
        return num;
    }

    std::string RxDevice::id2str(unsigned id)
    {
        return num2str(id);
    }
}
