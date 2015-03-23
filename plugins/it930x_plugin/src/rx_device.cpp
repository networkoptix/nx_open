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
#if 1
        printf("-- Can't lock channel for camera Tx: %d; Rx: %d (used: %d)\n", txID, m_rxID, m_sync.usedByCamera.load());
#endif
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
            m_device->lockFrequency( TxDevice::freq4chan(channel) );
            m_channel = channel;
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
#if 1
            printf("searching Tx. Rx: %d; frequency: %d; quality: %d; strength: %d; presence: %d\n",
                   rxID(), TxDevice::freq4chan(channel), quality(), strength(), present());
#endif
            if (good())
            {
                m_devReader->subscribe(It930x::PID_RETURN_CHANNEL);
                m_devReader->start(m_device.get(), true);
            }

            m_sync.searching.store(true);
        }
        else
        {
            printf("can't search, used. Rx: %d; frequency: %d\n", rxID(), TxDevice::freq4chan(channel));
        }
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
#if 1
                        printf("[RC] bad packet\n");
#endif
                        continue;
                    }

                    if (first)
                    {
                        first = false;
                        devLink.txID = pkt.txID();
                        devLink.channel = m_channel;

                        setTx(m_channel, devLink.txID);
                    }
#if 1
                    if (pkt.txID() != devLink.txID)
                        printf("-- Different TxIDs from one channel: %d vs %d\n", devLink.txID, pkt.txID());
                    if (! devLink.txID)
                        printf("txID == 0\n");
#endif
                }

                m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);
            }

            unlockC();
        }

        m_sync.searching.store(false);
    }

    void RxDevice::updateTxParams()
    {
        if (! m_txDev)
            return;

        for (;;)
        {
            RcPacketBuffer pktBuf;
            if (! m_devReader->getRcPacket(pktBuf))
                break;

            RcPacket pkt = pktBuf.packet();
            if (! pkt.isOK())
            {
                printf("[RC] bad packet\n");
                continue;
            }

            m_txDev->parse(pkt);
            return; ///< @note for better latency
        }

        uint16_t id = m_txDev->getWanted();
        if (id)
        {
            RcCommand * pcmd = m_txDev->mkRcCmd(id);
            if (pcmd && pcmd->isValid())
            {
                if (! sendRC(pcmd))
                    m_txDev->resetWanted();
            }
        }
    }

    bool RxDevice::changeChannel(unsigned chan)
    {
        if (m_txDev)
        {
            RcCommand * pcmd = m_txDev->mkSetChannel(chan);
            return sendRC(pcmd);
        }

        return false;
    }

    bool RxDevice::sendRC(RcCommand * cmd)
    {
        if (cmd && cmd->isValid() && m_txDev && m_device)
        {
            std::vector<RcPacketBuffer> pkts;

            {
                SendInfo& sinfo = m_txDev->sendInfo();

                std::lock_guard<std::mutex> lock(sinfo.mutex); // LOCK

                cmd->mkPackets(sinfo, m_rxID, pkts);
            }

            for (auto itPkt = pkts.begin(); itPkt != pkts.end(); ++itPkt)
                m_device->sendRcPacket(itPkt->packet());

            return true;
        }

        return false;
    }

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
