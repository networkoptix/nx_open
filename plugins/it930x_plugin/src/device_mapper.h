#ifndef ITE_DEVICE_MAPPER_H
#define ITE_DEVICE_MAPPER_H

#include <vector>
#include <map>

#include <plugins/camera_plugin.h>

#include "object_counter.h"

#include "rx_device.h"
#include "tx_device.h"

namespace ite
{
    //! Rx - Tx bidirectional mapper
    class DeviceMapper : public ObjectCounter<DeviceMapper>
    {
    public:
        struct DevLink
        {
            uint16_t rxID;
            uint16_t txID;
            unsigned frequency;
        };

        DeviceMapper();
        ~DeviceMapper();

        static void makeInfo(nxcip::CameraInfo& cameraInfo, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs);
        static void updateInfoAux(nxcip::CameraInfo& cameraInfo, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs);
        static void parseInfo(const nxcip::CameraInfo& cameraInfo, unsigned short& txID, unsigned& frequency, std::vector<unsigned short>& outRxIDs);
        void makeInfo(nxcip::CameraInfo& cameraInfo, unsigned short txID);

        void updateRxDevices();
        void updateTxDevices(unsigned chan);

        void getRx4Tx(unsigned short txID, std::vector<RxDevicePtr>& out) const; // called from CameraManager
        unsigned getFreq4Tx(unsigned short txID) const;

        void txDevs(std::vector<TxDevicePtr>& txDevs) const;
        void restoreCamera(const nxcip::CameraInfo& info);
        void getRestored(std::vector<DevLink>& links);

        void checkLink(DevLink& link);

    private:
        mutable std::mutex m_mutex;
        std::multimap<unsigned short, DevLink> m_devLinks;  // {TxID, RxTxLink}
        std::map<unsigned short, RxDevicePtr> m_rxDevs;     // {RxID, RxDev}
        std::map<unsigned short, TxDevicePtr> m_txDevs;     // {TxID, TxDev}
        std::vector<DevLink> m_restore;

        void checkLink(RxDevicePtr dev, DevLink& link);
        void addDevLink(const DevLink& link);
        void addTxDevice(const DevLink& link);

        static void getRxDevNames(std::vector<std::string>& names);
    };
}

#endif // ITE_DEVICE_MAPPER_H
