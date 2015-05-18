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
        resetFrozen();
    }

    bool RxDevice::open()
    {
        try
        {
            m_it930x.reset(); // dtor first
            m_it930x.reset(new It930x(m_rxID));
            m_it930x->info(m_rxInfo);
            return true;
        }
        catch (DtvException& ex)
        {
            debug_printf("[it930x] %s %d\n", ex.what(), ex.code());

            m_it930x.reset();
        }

        return false;
    }

    void RxDevice::resetFrozen()
    {
        try
        {
            if (m_it930x)
            {
                debug_printf("[it930x] Reset Rx device %d\n", m_rxID);
                m_it930x->rxReset();
            }
        }
        catch (DtvException& ex)
        {
            debug_printf("[it930x] %s %d\n", ex.what(), ex.code());

            m_it930x.reset();
        }
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
                        m_devReader->start(m_it930x.get());

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

        debug_printf("[camera] can't lock Rx: %d; Tx %d; channel: %d; used: %d\n",
                     m_rxID, txID, chan, m_sync.usedByCamera.load());
        return false;
    }

    bool RxDevice::tryLockC(unsigned channel, bool prio, const char ** reason)
    {
        if (channel >= RxDevice::NOT_A_CHANNEL)
            return false;

        if (! prio && wantedByCamera())
        {
            if (reason)
                *reason = "wanted by camera";
            return false;
        }

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (! prio && wantedByCamera())
        {
            if (reason)
                *reason = "wanted by camera";
            return false;
        }

        // prevent double locking at all (even with the same frequency)
        if (isLocked_u())
        {
            if (reason)
                *reason = "locked";
            return false;
        }

        if (!m_it930x && !open())
        {
            if (reason)
                *reason = "can't open Rx device";
            return false;
        }

        try
        {
            //static const unsigned QUALITY_TIMEOUT_MS = 1000;

            m_it930x->lockFrequency( TxDevice::freq4chan(channel) );

            //Timer::sleep(QUALITY_TIMEOUT_MS);
            stats();

            m_channel = channel;
            return true;
        }
        catch (DtvException& ex)
        {
            debug_printf("[it930x] %s %d\n", ex.what(), ex.code());

            m_it930x.reset();
            m_channel = NOT_A_CHANNEL;
            if (reason)
                *reason = "exception";
        }

        return false;
    }

    void RxDevice::unlockC(bool resetRx)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        m_devReader->stop();

        if (m_it930x)
            m_it930x->unlockFrequency();
        if (resetRx)
            resetFrozen();

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

    void RxDevice::stats()
    {
        if (!m_it930x)
            return;

        m_signalQuality = 0;
        m_signalStrength = 0;
        m_signalPresent = false;
        bool locked = false;
        m_it930x->statistic(m_signalQuality, m_signalStrength, m_signalPresent, locked);
    }

    bool RxDevice::startSearchTx(unsigned channel, unsigned timeoutMS)
    {
        if (m_sync.searching)
        {
            debug_printf("[search] BUG. Try to lock 2 channels same time. Rx: %d; channels: %d vs %d\n", rxID(), m_channel, channel);
            return false;
        }

        const char * reason;
        if (tryLockC(channel, false, &reason))
        {
            debug_printf("[search] %d sec. Rx: %d; channel: %d (%d); quality: %d; strength: %d; presence: %d\n",
                   timeoutMS/1000, rxID(), channel, TxDevice::freq4chan(channel), quality(), strength(), present());

            if (good())
            {
                m_devReader->subscribe(It930x::PID_RETURN_CHANNEL);
                m_devReader->start(m_it930x.get(), timeoutMS);

                m_sync.searching.store(true);
                return true;
            }

            unlockC();
            return false;
        }

        debug_printf("[search] can't lock Rx: %d; channel: %d (%d) - %s\n",
                    rxID(), channel, TxDevice::freq4chan(channel), reason ? reason : "unknown reason");
        return false;
    }

    void RxDevice::stopSearchTx(DevLink& devLink)
    {
        devLink.txID = 0;

        if (m_sync.searching /*&& isLocked()*/)
        {
            m_devReader->wait();

            bool first = true;
            for (;;)
            {
                RcPacketBuffer pktBuf;
                if (! m_devReader->getRcPacket(pktBuf))
                    break;

                if (pktBuf.pid() != It930x::PID_RETURN_CHANNEL)
                {
                    debug_printf("[RC] packet error: wrong PID\n");
                    continue;
                }

                if (pktBuf.checkbit())
                {
                    debug_printf("[RC] packet error: TS error bit\n");
                    continue;
                }

                RcPacket pkt = pktBuf.packet();
                if (! pkt.isOK())
                {
                    pkt.print();
                    continue;
                }

                if (pkt.txID() == 0 || pkt.txID() == 0xffff)
                {
                    debug_printf("[RC] wrong txID: %d\n", pkt.txID());
                    continue;
                }

                if (first && m_channel < NOT_A_CHANNEL)
                {
                    first = false;
                    devLink.rxID = m_rxID;
                    devLink.txID = pkt.txID();
                    devLink.channel = m_channel;

                    setTx(m_channel, devLink.txID);
                }

                if (pkt.txID() != devLink.txID)
                    debug_printf("[RC] different TxIDs from one channel: %d vs %d\n", devLink.txID, pkt.txID());
            }

            m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);
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
        else
            m_txDev->setReady();
    }

    bool RxDevice::sendRC(RcCommand * cmd)
    {
        TxDevicePtr txDev = m_txDev;

        bool ok = cmd && cmd->isValid() && txDev && m_it930x;
        if (!ok)
        {
            debug_printf("[RC send] bad command or device. RxID: %d TxID: %d\n", rxID(), txDev->txID());
            Timer::sleep(RC_DELAY_MS());
            return false;
        }

        uint16_t cmdID = cmd->commandID();

        if (! txDev->setWanted(cmdID))
        {
            debug_printf("[RC send] not sent - waiting response. RxID: %d TxID: %d cmd: 0x%x\n", rxID(), txDev->txID(), cmdID);
            Timer::sleep(RC_DELAY_MS());
            return false;
        }

        SendSequence& sseq = txDev->sendSequence();

        std::lock_guard<std::mutex> lock(sseq.mutex); // LOCK

        std::vector<RcPacketBuffer> pkts;
        cmd->mkPackets(sseq, m_rxID, pkts);

        try
        {
            /// @warning possible troubles with 1-port devices if multipacket
            for (auto itPkt = pkts.begin(); itPkt != pkts.end(); ++itPkt)
            {
                m_it930x->sendRcPacket(itPkt->packet());
                debug_printf("[RC send] OK. RxID: %d TxID: %d cmd: 0x%x\n", rxID(), txDev->txID(), cmdID);
            }
        }
        catch (DtvException& ex)
        {
            debug_printf("[RC send][it930x] %s %d\n", ex.what(), ex.code());
        }

        return true;
    }

    bool RxDevice::sendRC(RcCommand * cmd, unsigned attempts)
    {
        for (unsigned i = 0; i < attempts; ++i)
        {
            if (sendRC(cmd)) // delay inside
                return true;
        }

        return false;
    }

    //

    void RxDevice::updateOSD()
    {
        if (m_txDev)
        {
            RcCommand * pcmd = m_txDev->mkSetOSD(0);
            sendRC(pcmd, RC_ATTEMPTS());

            pcmd = m_txDev->mkSetOSD(1);
            sendRC(pcmd, RC_ATTEMPTS());

            pcmd = m_txDev->mkSetOSD(2);
            sendRC(pcmd, RC_ATTEMPTS());
        }
    }

    bool RxDevice::changeChannel(unsigned chan)
    {
        if (m_txDev)
        {
            RcCommand * pcmd = m_txDev->mkSetChannel(chan);
            return sendRC(pcmd, RC_ATTEMPTS());
        }

        return false;
    }

    bool RxDevice::setEncoderParams(unsigned streamNo, unsigned fps, unsigned bitrateKbps)
    {
        if (m_txDev)
        {
            RcCommand * pcmd = m_txDev->mkSetEncoderParams(streamNo, fps, bitrateKbps);
            return sendRC(pcmd, RC_ATTEMPTS());
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
