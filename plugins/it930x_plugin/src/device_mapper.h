#ifndef ITE_DEVICE_MAPPER_H
#define ITE_DEVICE_MAPPER_H

#include <map>

#include <plugins/camera_plugin.h>

#include "object_counter.h"

#include "rx_device.h"
#include "tx_device.h"

namespace ite
{
    /// RX-TX mapper
    class DeviceMapper : public ObjectCounter<DeviceMapper>
    {
    public:
        DeviceMapper();
        ~DeviceMapper();

        void updateRxDevices();
        void updateTxDevices();
        void restoreTxDevices();

        RxDevicePtr getRx(uint16_t rxID);

        // from DiscoveryManager

        void restoreCamera(const nxcip::CameraInfo& info);
        void makeInfo(nxcip::CameraInfo& cameraInfo, unsigned short txID);
        void txDevs(std::vector<TxDevicePtr>& txDevs) const;

        // from CameraManager

        static void parseInfo(const nxcip::CameraInfo& cameraInfo, unsigned short& txID, unsigned& frequency, std::vector<unsigned short>& outRxIDs);

        void getRx4Tx(unsigned short txID, std::vector<RxDevicePtr>& out) const;
        unsigned freq4Tx(unsigned short txID) const;

    private:
        mutable std::mutex m_mutex;
        std::map<unsigned short, RxDevicePtr> m_rxDevs;     // {RxID, RxDev}
        std::map<unsigned short, TxDevicePtr> m_txDevs;     // {TxID, TxDev}

        void addTxDevice(const DevLink& link);

        static void getRxDevNames(std::vector<std::string>& names);

        static void makeInfo(nxcip::CameraInfo& cameraInfo, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs);
        static void updateInfoAux(nxcip::CameraInfo& cameraInfo, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs);
    };
}

#endif // ITE_DEVICE_MAPPER_H
