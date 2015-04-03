#include <dirent.h>

#include <memory>
#include <string>
#include <atomic>
#include <sstream>
#include <thread>

#include "device_mapper.h"

#ifdef COUNT_OBJECTS
#include "device_mapper.h"
#include "discovery_manager.h"
#include "camera_manager.h"
#include "media_encoder.h"
#include "stream_reader.h"
#include "video_packet.h"

namespace
{
    void printCounters()
    {
        printf("\n");
        printCtorDtor<ContentPacket>();
        printCtorDtor<Demux>();
        printCtorDtor<DeviceBuffer>();
        printCtorDtor<DevReader>();
        printCtorDtor<DeviceMapper>();

        printCtorDtor<DiscoveryManager>();
        printCtorDtor<CameraManager>();
        printCtorDtor<MediaEncoder>();
        printCtorDtor<StreamReader>();
        printCtorDtor<VideoPacket>();
        printf("\n");
    }

    template <typename T>
    void printCtorDtor()
    {
        typedef ObjectCounter<T> Counter;

        printf("%s:\t%d - %d = %d\n", Counter::name(), Counter::ctorCount(), Counter::dtorCount(), Counter::diffCount());
    }
}
#endif

namespace ite
{
    INIT_OBJECT_COUNTER(DeviceMapper)

    class RC_DiscoveryThread;
    static RC_DiscoveryThread * updateThreadObj = nullptr;
    static std::thread updateThread;

    ///
    class RC_DiscoveryThread
    {
    public:
        RC_DiscoveryThread(DeviceMapper * devMapper)
        :   m_stopMe(false),
            m_devMapper(devMapper)
        {}

        RC_DiscoveryThread(const RC_DiscoveryThread& rct)
        :   m_stopMe(false),
            m_devMapper(rct.m_devMapper)
        {}

        void operator () ()
        {
            static const unsigned RESTORE_DELAY_S = 4;
            static std::chrono::seconds delay(RESTORE_DELAY_S);

            updateThreadObj = this;

            std::this_thread::sleep_for(delay);

            while (!m_stopMe)
            {
                m_devMapper->updateRxDevices();
                m_devMapper->updateTxDevices();
            }

            updateThreadObj = nullptr;
        }

        void stop() { m_stopMe = true; }

    private:
        std::atomic_bool m_stopMe;
        DeviceMapper * m_devMapper;
    };

    //

    DeviceMapper::DeviceMapper()
    {
        try
        {
            updateRxDevices();

            updateThread = std::thread( RC_DiscoveryThread(this) );
        }
        catch (std::system_error& )
        {}
    }

    DeviceMapper::~DeviceMapper()
    {
        if (updateThreadObj)
        {
            updateThreadObj->stop();
            updateThread.join();
        }
    }

    void DeviceMapper::getRx4Tx(unsigned short txID, std::vector<RxDevicePtr>& devs) const
    {
        devs.clear();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (auto it = m_rxDevs.begin(); it != m_rxDevs.end(); ++it)
        {
            unsigned chan = it->second->chan4Tx(txID);
            if (chan < TxDevice::CHANNELS_NUM)
                devs.push_back(it->second);
        }
    }

    void DeviceMapper::forgetTx(unsigned short txID)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (auto it = m_rxDevs.begin(); it != m_rxDevs.end(); ++it)
            it->second->forgetTx(txID);
    }

    unsigned DeviceMapper::freq4Tx(unsigned short txID) const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        auto it = m_txDevs.find(txID);
        if (it != m_txDevs.end())
            return it->second->frequency();
        return 0;
    }

    void DeviceMapper::getRxDevNames(std::vector<std::string>& devs)
    {
        devs.clear();

        DIR * dir = ::opendir( "/dev" );
        if (dir != NULL)
        {
            struct dirent * ent;
            while ((ent = ::readdir( dir )) != NULL)
            {
                std::string file( ent->d_name );
                if (file.find( RxDevice::DEVICE_PATTERN ) != std::string::npos)
                    devs.push_back(file);
            }
            ::closedir( dir );
        }
    }

    void DeviceMapper::updateRxDevices()
    {
        std::vector<std::string> names;
        getRxDevNames(names);

        std::map<unsigned short, RxDevicePtr> rxDevs;

        for (size_t i = 0; i < names.size(); ++i)
        {
            unsigned rxID = RxDevice::dev2id(names[i]);

            auto it = m_rxDevs.find(rxID);
            if (it != m_rxDevs.end())
                rxDevs[rxID] = it->second;
            else
                rxDevs[rxID] = std::make_shared<RxDevice>(rxID); // create RxDevice
        }

        debug_printf("got RX devices: %ld\n", rxDevs.size());
#if 1
        for (size_t i = 0; i < rxDevs.size(); ++i)
        {
            debug_printf("rx: %d tx:", rxDevs[i]->rxID());
            for (unsigned ch = 0; ch < TxDevice::CHANNELS_NUM; ++ch)
            {
                unsigned txID = rxDevs[i]->getTx(ch);
                if (txID)
                    debug_printf(" %d", txID);
            }
            debug_printf("\n");
        }
#endif
        size_t numTx = 0;
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            m_rxDevs.swap(rxDevs);
            numTx = m_txDevs.size();
        }

        debug_printf("got TX devices: %ld\n", numTx);
    }

    void DeviceMapper::updateTxDevices()
    {
        std::vector<RxDevicePtr> scanDevs;
        std::stringstream ssFreeDevs;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            for (auto it = m_rxDevs.begin(); it != m_rxDevs.end(); ++it)
            {
                if (it->second.get() && ! it->second->isLocked())
                {
                    scanDevs.push_back(it->second);
                    ssFreeDevs << ' ' << it->second->rxID();
                }
            }
        }

        if (scanDevs.empty())
        {
            debug_printf("Start Tx search. All Rx are busy. Do nothing\n");
            Timer::sleep(1000);
            return;
        }

        debug_printf("Scan for Tx. Free Rx:%s\n", ssFreeDevs.str().c_str());

        std::vector<DevLink> links;
        std::map<uint16_t, std::vector<bool>> rescan; // {i, chan[N]}

        // normal channel search
        static const unsigned SCAN_TIMEOUT_MS = 4000;
        for (unsigned chan = 0; chan < TxDevice::CHANNELS_NUM; ++chan)
        {
            std::vector<bool> good(scanDevs.size(), false);

            for (size_t i = 0; i < scanDevs.size(); ++i)
                good[i] = scanDevs[i]->startSearchTx(chan, SCAN_TIMEOUT_MS);

            for (size_t i = 0; i < scanDevs.size(); ++i)
            {
                DevLink link;
                scanDevs[i]->stopSearchTx(link);
                if (link.txID)
                    links.push_back(link);

                if (good[i] && ! link.txID)
                {
                    rescan[i].resize(TxDevice::CHANNELS_NUM);
                    rescan[i][chan] = true;
                }
            }

            for (auto it = links.begin(); it != links.end(); ++it)
                addTxDevice(*it);
            links.clear();
        }

        if (rescan.size())
            debug_printf("Rescan started\n");

        /// @note workaround: rescan good() Rx with [RC] bad packets
        static const unsigned RESCAN_TIMEOUT_MS = 16000;
        for (unsigned chan = 0; chan < TxDevice::CHANNELS_NUM; ++chan)
        {
            for (size_t i = 0; i < scanDevs.size(); ++i)
            {
                auto it = rescan.find(i);
                if (it != rescan.end() && it->second.size() && it->second[chan])
                    scanDevs[i]->startSearchTx(chan, RESCAN_TIMEOUT_MS);
            }

            for (size_t i = 0; i < scanDevs.size(); ++i)
            {
                DevLink link;
                scanDevs[i]->stopSearchTx(link);
                if (link.txID)
                    links.push_back(link);
            }

            for (auto it = links.begin(); it != links.end(); ++it)
                addTxDevice(*it);
            links.clear();
        }

        if (rescan.size())
            debug_printf("Rescan finished\n");
    }

    void DeviceMapper::restoreCamera(const nxcip::CameraInfo& info)
    {
        DevLink link;

        std::vector<unsigned short> rxIDs;
        unsigned freq;
        DeviceMapper::parseInfo(info, link.txID, freq, rxIDs);
        link.channel = TxDevice::chan4freq(freq);

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (auto it = rxIDs.begin(); it != rxIDs.end(); ++it)
        {
            link.rxID = *it;

            auto itDev = m_rxDevs.find(link.rxID);
            if (itDev != m_rxDevs.end())
            {
                RxDevicePtr dev = itDev->second;
                if (dev)
                    dev->checkTx(link.channel, link.txID);
            }
        }
    }

    RxDevicePtr DeviceMapper::getRx(uint16_t rxID)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        auto it = m_rxDevs.find(rxID);
        if (it != m_rxDevs.end())
            return it->second;

        return RxDevicePtr();
    }

    void DeviceMapper::addTxDevice(const DevLink& link)
    {
        debug_printf("device link Rx: %d; Tx: %d; channel: %d\n", link.rxID, link.txID, link.channel);

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        unsigned freq = TxDevice::freq4chan(link.channel);

        auto it = m_txDevs.find(link.txID);
        if (it == m_txDevs.end())
            m_txDevs[link.txID] = std::make_shared<TxDevice>(link.txID, freq);
        else
            it->second->setFrequency(freq);
    }

    void DeviceMapper::txDevs(std::vector<TxDevicePtr>& txDevs) const
    {
        txDevs.clear();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        txDevs.reserve(m_txDevs.size());
        for (auto it = m_txDevs.begin(); it != m_txDevs.end(); ++it)
            txDevs.push_back(it->second);
    }

    // CameraInfo stuff

    void DeviceMapper::makeInfo(nxcip::CameraInfo& info, unsigned short txID)
    {
        std::vector<RxDevicePtr> rxDevs;
        getRx4Tx(txID, rxDevs);

        std::vector<unsigned short> rxIDs;
        for (auto it = rxDevs.begin(); it != rxDevs.end(); ++it)
            rxIDs.push_back((*it)->rxID());

        unsigned freq = freq4Tx(txID);
        makeInfo(info, txID, freq, rxIDs);
    }

    void DeviceMapper::makeInfo(nxcip::CameraInfo& info, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs)
    {
        std::string strTxID = RxDevice::id2str(txID);

        memset( &info, 0, sizeof(nxcip::CameraInfo) );
        strncpy( info.modelName, "Pacidal", sizeof(nxcip::CameraInfo::modelName)-1 ); // TODO
        strncpy( info.url, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::url)-1) );
        strncpy( info.uid, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::uid)-1) );

        updateInfoAux(info, txID, frequency, rxIDs);
    }

    void DeviceMapper::updateInfoAux(nxcip::CameraInfo& info, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs)
    {
        std::stringstream ss;
        ss << txID << ' ' << frequency;

        for (size_t i = 0; i < rxIDs.size(); ++i)
            ss << ' ' << rxIDs[i];

        //memset(&info.auxiliaryData, 0, sizeof(nxcip::CameraInfo::auxiliaryData));
        unsigned len = std::min(ss.str().size(), sizeof(nxcip::CameraInfo::auxiliaryData)-1);
        strncpy(info.auxiliaryData, ss.str().c_str(), len);
        info.auxiliaryData[len] = 0;
    }

    void DeviceMapper::parseInfo(const nxcip::CameraInfo& info, unsigned short& txID, unsigned& frequency, std::vector<unsigned short>& outRxIDs)
    {
        if (info.auxiliaryData[0])
        {
            std::stringstream ss;
            ss << std::string(info.auxiliaryData);
            ss >> txID >> frequency;

            while (ss.rdbuf()->in_avail())
            {
                unsigned short rxID;
                ss >> rxID;
                outRxIDs.push_back(rxID);
            }
        }
    }
}
