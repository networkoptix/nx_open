#ifndef ITE_DEVICE_H
#define ITE_DEVICE_H

#include <memory>
#include <mutex>

extern "C"
{
#if defined( DTVAPI_V14 )
 #include "dtvapi_v14/DTVAPI.h"
#elif defined( DTVAPI_V15 )
 #include "dtvapi_v15/DTVAPI.h"
#endif
}

#include "rc_command.h"

namespace ite
{
    struct IteDriverInfo
    {
        char driverVersion[16];
        char APIVersion[32];
        char fwVersionLink[16];
        char fwVersionOFDM[16];
        char company[8];
        char supportHWInfo[32];
    };

    struct Pacidal
    {
        enum
        {
            PID_VIDEO_FHD = 0x111,
            PID_VIDEO_HD = 0x121,
            PID_VIDEO_SD = 0x131,
            PID_VIDEO_CIF = 0x141
        };
    };

    /// ITE DVB-T receiver
    ///
    /// RX Setting (from driver readme):
    /// URB_COUNT   4
    /// URB_BUFSIZE 153408
    /// GETDATATIMEOUT: 3000ms
    ///
    class It930x
    {
    public:
        enum
        {
            PID_RETURN_CHANNEL = 0x201
        };

        typedef enum
        {
            Bandwidth_5M = 5000,    // Signal bandwidth is 5MHz
            Bandwidth_6M = 6000,    // Signal bandwidth is 6MHz
            Bandwidth_7M = 7000,    // Signal bandwidth is 7MHz
            Bandwidth_8M = 8000     // Signal bandwidth is 8MHz
        } Bandwidth;

        class It930Stream
        {
        public:
            It930Stream(const It930x * p)
            :   m_parent(p)
            {
                if (DTV_StartCapture(m_parent->m_handle))
                    throw "DTV_StartCapture";
            }

            ~It930Stream()
            {
                DTV_StopCapture(m_parent->m_handle);
            }

            ///
            /// \return -1 error
            /// \return 0 no data
            ///
            int read(uint8_t * buf, int size)
            {
                return DTV_GetData(m_parent->m_handle, size, buf);
            }

        private:
            const It930x * m_parent;
        };

        ///
        typedef enum
        {
            NullPacketFilter = 0,
            PIDTable
        } Option;

        //

        It930x(uint16_t rxID)
        :   m_rxID(rxID)
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            m_handle = DTV_DeviceOpen(ENDEAVOUR, rxID);
            if (m_handle < 0)
                throw "DTV_DeviceOpen";
        }

        ~It930x()
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            m_devStream.reset();
            DTV_DeviceClose(m_handle);
        }
#if 0
        void enable(Option opt)
        {
            switch (opt)
            {
            case NullPacketFilter:
                setNullPacketFilter(true);
                break;

            case PIDTable:
                if (DTV_EnablePIDTable(m_handle))
                    throw "DTV_EnablePIDTable";
                break;
            }
        }

        void disable(Option opt)
        {
            switch (opt)
            {
            case NullPacketFilter:
                setNullPacketFilter(false);
                break;

            case PIDTable:
                if (DTV_DisablePIDTable(m_handle))
                    throw "DTV_DisablePIDTable";
                break;
            }
        }

        /// @param idx = 0..31
        bool pidFilterAdd(uint8_t idx, short programID) { return 0 == DTV_AddPID(m_handle, idx, programID); }
        bool pidFilterRemove(uint8_t idx, short programID) { return 0 == DTV_RemovePID(m_handle, idx, programID); }
        bool pidFilterReset() { return 0 == DTV_ResetPIDTable(m_handle); }
#endif
        // both in KHz
        void lockFrequency(unsigned frequency, unsigned bandwidth = Bandwidth_6M)
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            if (m_devStream)
                throw "It930x.lockFrequency() already locked";

            unsigned result = DTV_AcquireChannel(m_handle, frequency, bandwidth);
            if (result)
                throw "DTV_AcquireChannel";

            Bool isLocked = False;
            result = DTV_IsLocked(m_handle, &isLocked);
            if (result)
                throw "DTV_IsLocked";

            m_devStream.reset( new It930Stream(this) );
        }

        void unlockFrequency()
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            m_devStream.reset();
        }
#if 0
        bool isLocked() const
        {
            Bool locked = False;
            unsigned result = DTV_IsLocked(m_handle, &locked);
            return !result && (locked == True);
        }
#endif
        void statistic(uint8_t& quality, uint8_t& strength, bool& presented, bool& locked) const
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            DTVStatistic statisic;
            unsigned result = DTV_GetStatistic(m_handle, &statisic);
            if (result)
                throw "DTV_GetStatistic";

            quality = statisic.signalQuality;
            strength = statisic.signalStrength;
            presented = statisic.signalPresented;
            locked = statisic.signalLocked;
        }

        void info(IteDriverInfo& info)
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            memset(&info, 0, sizeof(IteDriverInfo));

            DemodDriverInfo driverInfo;
            int result = DTV_GetDriverInformation(m_handle, &driverInfo);
            if (result)
                return;

            strncpy(info.driverVersion, (const char *)driverInfo.DriverVersion, 16);
            strncpy(info.APIVersion,    (const char *)driverInfo.APIVersion, 32);
            strncpy(info.fwVersionLink, (const char *)driverInfo.FWVersionLink, 16);
            strncpy(info.fwVersionOFDM, (const char *)driverInfo.FWVersionOFDM, 16);
            strncpy(info.company,       (const char *)driverInfo.Company, 8);
            strncpy(info.supportHWInfo, (const char *)driverInfo.SupportHWInfo, 32);
        }

        void sendRcPacket(const RcPacket& cmd)
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            int result = DTV_SendRC(m_handle, cmd.rawData(), cmd.rawSize());
            if (result)
                throw "DTV_SendCommand";
        }

        int read(uint8_t * buf, int size)
        {
            //std::lock_guard<std::mutex> lock(m_mutex); // LOCK

            if (m_devStream)
                return m_devStream->read(buf, size);
            return -1;
        }

        bool hasStream() const { return m_devStream.get(); }

    private:
        static std::mutex m_rcMutex;
        int m_handle;
        uint16_t m_rxID;
        std::unique_ptr<It930Stream> m_devStream;

#if 0 // TODO: use it instead of ID from unix device name
        void getDeviceID() const
        {
            Word rxDeviceID = 0xFFFF;
            unsigned result = DTV_GetDeviceID(handle_, &rxDeviceID);
            if (result)
                throw "DTV_GetDeviceID";
        }

        void setNullPacketFilter(bool flag)
        {
            PIDFilter pidFilter;
            pidFilter.oldPID[0] = 0x1FFF;
            pidFilter.newPID[0] = 0x1FFF;
            pidFilter.tableLen = (flag) ? 1 : 0;

            unsigned result = DTV_NULLPacket_Pilter(m_handle, &pidFilter);
            if (result)
                throw "NULL Packet Filter";
        }
#endif
    };
}

#endif // ITE_DEVICE_H
