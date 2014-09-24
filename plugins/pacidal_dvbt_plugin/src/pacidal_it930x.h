#ifndef PACIDAL_DEVICE_H
#define PACIDAL_DEVICE_H

extern "C"
{
#include "DTVAPI.h"
}

namespace pacidal
{
    class PacidalStream;

    /// Pacidal DVB-T reciver
    ///
    /// RX Setting (from driver readme):
    /// URB_COUNT   4
    /// URB_BUFSIZE 153408
    /// GETDATATIMEOUT: 3000ms
    ///
    class PacidalIt930x
    {
        friend class PacidalStream;

    public:
        static const unsigned MPEG_TS_PACKET_SIZE = 188;
        static const unsigned DEFAULT_PACKETS_NUM = 816; // or 348

        typedef enum
        {
            Bandwidth_5M = 5000,    // Signal bandwidth is 5MHz
            Bandwidth_6M = 6000,    // Signal bandwidth is 6MHz
            Bandwidth_7M = 7000,    // Signal bandwidth is 7MHz
            Bandwidth_8M = 8000     // Signal bandwidth is 8MHz
        } Bandwidth;

        ///
        typedef enum
        {
            NullPacketFilter = 0,
            PIDTable
        } Option;

        static unsigned bufferSize(unsigned numPackets = DEFAULT_PACKETS_NUM) { return numPackets * MPEG_TS_PACKET_SIZE; }

        //

        PacidalIt930x(uint8_t number)
        {
            handle_ = DTV_DeviceOpen(ENDEAVOUR, number);
            if (handle_ < 0)
                throw "DTV_DeviceOpen";
        }

        ~PacidalIt930x()
        {
           DTV_DeviceClose(handle_);
        }

        void enable(Option opt)
        {
            switch (opt)
            {
            case NullPacketFilter:
                setNullPacketFilter(true);
                break;

            case PIDTable:
                if (DTV_EnablePIDTable(handle_))
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
                if (DTV_DisablePIDTable(handle_))
                    throw "DTV_DisablePIDTable";
                break;
            }
        }

        /// @param idx = 0..31
        bool pidFilterAdd(uint8_t idx, short programID) { return 0 == DTV_AddPID(handle_, idx, programID); }
        bool pidFilterRemove(uint8_t idx, short programID) { return 0 == DTV_RemovePID(handle_, idx, programID); }
        bool pidFilterReset() { return 0 == DTV_ResetPIDTable(handle_); }

        // both in KHz
        void lockChannel(unsigned frequency, unsigned bandwidth = Bandwidth_6M)
        {
            unsigned result = DTV_AcquireChannel(handle_, frequency, bandwidth);
            if (result)
                throw "DTV_AcquireChannel";

            Bool isLocked = False;
            result = DTV_IsLocked(handle_, &isLocked);
            if (result)
                throw "DTV_IsLocked";
        }

        bool isLocked() const
        {
            Bool locked = False;
            unsigned result = DTV_IsLocked(handle_, &locked);
            return !result && (locked == True);
        }

        void statistic(bool& locked, bool& presented, uint8_t& strength, uint8_t& abortCount, float& ber) const
        {
            DTVStatistic statisic;

            unsigned result = DTV_GetStatistic(handle_, &statisic);
            if (result)
                throw "DTV_GetStatistic";

            locked = statisic.signalLocked;
            presented = statisic.signalPresented;
            strength = statisic.signalStrength;
            abortCount = statisic.abortCount;
            ber = statisic.postVitErrorCount;
            if( statisic.postVitBitCount )
                ber /= statisic.postVitBitCount;
        }

        void info(char* verDriver, char* verAPI, char* verFWLink, char* verFWOFDM, char* company, char* model)
        {
            DemodDriverInfo driverInfo;
            int result = DTV_GetDriverInformation(handle_, &driverInfo);
            if (result)
                return;

            strncpy(verDriver,  (const char *)driverInfo.DriverVersion, 16);
            strncpy(verAPI,     (const char *)driverInfo.APIVersion, 32);
            strncpy(verFWLink,  (const char *)driverInfo.FWVersionLink, 16);
            strncpy(verFWOFDM,  (const char *)driverInfo.FWVersionOFDM, 16);
            strncpy(company,    (const char *)driverInfo.Company, 8);
            strncpy(model,      (const char *)driverInfo.SupportHWInfo, 32);
        }

    private:
        int handle_;
#if 0
        void getDeviceID() const
        {
            Word rxDeviceID = 0xFFFF;
            unsigned result = DTV_GetDeviceID(handle_, &rxDeviceID);
            if (result)
                throw "DTV_GetDeviceID";
        }
#endif
        void setNullPacketFilter(bool flag)
        {
            PIDFilter pidFilter;
            pidFilter.oldPID[0] = 0x1FFF;
            pidFilter.newPID[0] = 0x1FFF;
            pidFilter.tableLen = (flag) ? 1 : 0;

            unsigned result = DTV_NULLPacket_Pilter(handle_, &pidFilter);
            if (result)
                throw "NULL Packet Filter";
        }
    };

    ///
    class PacidalStream
    {
    public:
        PacidalStream(const PacidalIt930x& p)
        :   parent_(&p)
        {
            if (DTV_StartCapture(parent_->handle_))
                throw "DTV_StartCapture";
        }

        ~PacidalStream()
        {
            DTV_StopCapture(parent_->handle_);
        }

        ///
        /// \return -1 error
        /// \return 0 no data
        ///
        int read(uint8_t * buf, int size)
        {
            return DTV_GetData(parent_->handle_, size, buf);
        }

    private:
        const PacidalIt930x * parent_;
    };
}

#endif //PACIDAL_DEVICE_H
