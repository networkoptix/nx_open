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
        m_txID(0),
        m_frequency(0),
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

    bool RxDevice::lockCamera(unsigned short txID, unsigned freq)
    {
        typedef std::chrono::steady_clock Clock;

        // TODO: class Timer
        static const unsigned DURATION_MS = TxDevice::SEND_WAIT_TIME_MS * 2;
        static const unsigned DELAY_MS = 20;

        std::chrono::milliseconds duration(DURATION_MS);
        std::chrono::milliseconds delay(DELAY_MS);

        // could be locked by Discovery, waiting
        std::chrono::time_point<Clock> timeFinish = Clock::now() + duration;
        while (Clock::now() < timeFinish)
        {
            if (m_txDev) // locked by another camera
                return false;

            if (tryLockF(freq))
            {
                m_txID = txID;
                updateTxParams();
                if (good())
                    m_devReader->start(m_device.get());
                return true;
            }

            std::this_thread::sleep_for(delay);
        }

        return false;
    }

    bool RxDevice::tryLockF(unsigned freq)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        // prevent double locking at all (even with the same frequency)
        if (isLocked_u())
            return false;

        if (!m_device && !open())
            return false;

        try
        {
            m_device->lockChannel(freq);
            m_frequency = freq;
            stats();

            if (good())
                m_devReader->setDevice(m_device.get());
            return true;
        }
        catch (const char * msg)
        {
            m_devReader->stop();
            m_device.reset();
        }

        return false;
    }

    void RxDevice::unlockF()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (m_devReader)
            m_devReader->stop();

        if (m_device)
            m_device->closeStream();

        m_txDev.reset();
        m_frequency = 0;
        m_txID = 0;
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

    bool RxDevice::findTx(unsigned freq, uint16_t& outTxID)
    {
        typedef std::chrono::steady_clock Clock;

        // TODO: class Timer
        static const unsigned RET_CHAN_SEARCH_MS = 1000;
        std::chrono::milliseconds duration(RET_CHAN_SEARCH_MS);

        bool retVal = false;
        outTxID = 0;

        if (tryLockF(freq))
        {
#if 1
            printf("searching TxIDs - rxID: %d; frequency: %d; quality: %d; strength: %d; presence: %d\n",
                   rxID(), freq, quality(), strength(), present());
#endif
            if (good())
            {
                std::chrono::time_point<Clock> timeFinish = Clock::now() + duration;

                while (Clock::now() < timeFinish)
                {
                    RCCommand cmd;
                    if (m_devReader->readRetChanCmd(cmd))
                    {
                        if (cmd.isOK())
                        {
                            retVal = true;
                            outTxID = cmd.txID();
                            break;
                        }
                    }
                }
            }

            unlockF();
        }

#if 1
        if (retVal)
            printf("Rx: %d; Tx: %x (%d)\n", rxID(), outTxID, outTxID);
#endif
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
    bool RxDevice::setChannel(unsigned short txID, unsigned chan)
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
