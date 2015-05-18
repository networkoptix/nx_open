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

    ///
    class DtvException
    {
    public:
        /// @warning static strings only
        DtvException(const char * msg, unsigned code)
        :   m_msg(msg),
            m_code(code)
        {}

        const char * what() const { return m_msg; }
        unsigned code() const { return m_code; }

    private:
        const char * m_msg;
        unsigned m_code;
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
                unsigned ret = DTV_StartCapture(m_parent->m_handle);
                if (ret)
                    throw DtvException("DTV_StartCapture", ret);
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
                throw DtvException("DTV_DeviceOpen", rxID);
        }

        ~It930x()
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            m_devStream.reset();
            DTV_DeviceClose(m_handle);
        }

        // both in KHz
        void lockFrequency(unsigned frequency, unsigned bandwidth = Bandwidth_6M)
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            if (m_devStream)
                throw DtvException("It930x.lockFrequency()", 0);

            unsigned ret = DTV_AcquireChannel(m_handle, frequency, bandwidth);
            if (ret)
                throw DtvException("DTV_AcquireChannel", ret);

            Bool isLocked = False;
            ret = DTV_IsLocked(m_handle, &isLocked);
            if (ret)
                throw DtvException("DTV_IsLocked", ret);

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
            unsigned ret = DTV_GetStatistic(m_handle, &statisic);
            if (ret)
                throw DtvException("DTV_GetStatistic", ret);

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
            int ret = DTV_GetDriverInformation(m_handle, &driverInfo);
            if (ret)
                throw DtvException("DTV_GetDriverInformation", ret);

            strncpy(info.driverVersion, (const char *)driverInfo.DriverVersion, 16);
            strncpy(info.APIVersion,    (const char *)driverInfo.APIVersion, 32);
            strncpy(info.fwVersionLink, (const char *)driverInfo.FWVersionLink, 16);
            strncpy(info.fwVersionOFDM, (const char *)driverInfo.FWVersionOFDM, 16);
            strncpy(info.company,       (const char *)driverInfo.Company, 8);
            strncpy(info.supportHWInfo, (const char *)driverInfo.SupportHWInfo, 32);
        }

        void rxReset()
        {
#if DTVAPI_V15
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            Byte value = 1; // some magic value from sample
            int ret = DTV_RX_Reset(m_handle, &value);
            if (ret)
                throw DtvException("DTV_RX_Reset", ret);
#endif
        }

        void sendRcPacket(const RcPacket& cmd)
        {
            std::lock_guard<std::mutex> lock(m_rcMutex); // LOCK

            int ret = DTV_SendRC(m_handle, cmd.rawData(), cmd.rawSize());
            if (ret)
                throw DtvException("DTV_SendRC", ret);
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

#if 0
        void getDeviceID() const
        {
            Word rxDeviceID = 0xFFFF;
            unsigned ret = DTV_GetDeviceID(m_handle, &rxDeviceID);
            if (ret)
                throw DtvException("DTV_GetDeviceID", ret);
        }

        void setNullPacketFilter(bool flag)
        {
            PIDFilter pidFilter;
            pidFilter.oldPID[0] = 0x1FFF;
            pidFilter.newPID[0] = 0x1FFF;
            pidFilter.tableLen = (flag) ? 1 : 0;

            unsigned ret = DTV_NULLPacket_Pilter(m_handle, &pidFilter);
            if (ret)
                throw DtvException("DTV_NULLPacket_Pilter", ret);
        }
#endif
    };
}

#endif // ITE_DEVICE_H
