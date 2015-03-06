#include <sstream>
#include <thread>

#include "timer.h"
#include "rx_device.h"
#include "mpeg_ts_packet.h"

namespace ite
{
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

    bool RxDevice::lockCamera(uint16_t txID)
    {
        using std::chrono::system_clock;

        static const unsigned TIMEOUT_MS = 5000;
        static const std::chrono::milliseconds timeout(TIMEOUT_MS);

        unsigned chan = chan4Tx(txID);

        // lock only one camera
        bool expected = false;
        if (m_sync.usedByCamera.compare_exchange_strong(expected, true))
        {
            for (unsigned i = 0; i < 2; ++i)
            {

                if (tryLockC(chan))
                {
                    updateTxParams();
                    if (good())
                        m_devReader->start(m_device.get());
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

    bool RxDevice::tryLockC(unsigned channel)
    {
        if (channel >= RxDevice::NOT_A_CHANNEL)
            return false;

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

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
            m_device->closeStream();

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

    bool RxDevice::startSearchTx(unsigned channel)
    {
        bool cantSearch = false;
        {
            std::unique_lock<std::mutex> cvLock( m_sync.mutex ); // LOCK

            if (m_sync.waiting)
                cantSearch = true;
        }

        if (cantSearch || m_sync.usedByCamera)
        {
            printf("cant't search, used. Rx: %d; frequency: %d\n", rxID(), TxDevice::freq4chan(channel));
            return false;
        }

        if (tryLockC(channel))
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

            return true;
        }

        return false;
    }

    bool RxDevice::stopSearchTx(uint16_t& outTxID)
    {
        bool retVal = false;
        outTxID = 0;

        if (isLocked())
        {
            if (good())
            {
                m_devReader->wait();

                RCCommand cmd;
                if (m_devReader->getRetChanCmd(cmd) && cmd.isOK())
                {
                    retVal = true;
                    outTxID = cmd.txID();
                    setTx(m_channel, outTxID);

                    while (m_devReader->getRetChanCmd(cmd))
                    {
                        if (cmd.isOK() && cmd.txID() != outTxID)
                            printf("-- Different TxIDs from one channel: %d vs %d\n", outTxID, cmd.txID());
                    }
                }

                m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);
            }

            unlockC();
        }

        return retVal;
    }

    // TODO
    void RxDevice::updateTxParams()
    {
#if 0
        m_txDev = m_rcShell->device(m_rxID, m_txID);
        if (m_txDev)
        {
            /// @note async
            m_rcShell->updateTxParams(m_txInfo);
        }
#endif
    }

    // TODO
    bool RxDevice::changeChannel(unsigned short txID, unsigned chan)
    {
#if 0
        if (isLocked())
            return m_rcShell->setChannel(rxID(), txID, chan);
#endif
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
