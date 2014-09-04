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

        void statistic(bool& locked, bool& presented, uint8_t& quality, uint8_t& strength) const
        {
            DTVStatistic statisic;

            unsigned result = DTV_GetStatistic(handle_, &statisic);
            if (result)
                throw "DTV_GetStatistic";

            locked = statisic.signalLocked;
            presented = statisic.signalPresented;
            quality = statisic.signalQuality;
            strength = statisic.signalStrength;
        }

    private:
        int handle_;
#if 0
        void printDriverInfo() const
        {
            Word rxDeviceID = 0xFFFF;
            unsigned result = DTV_GetDeviceID(handle_, &rxDeviceID);
            if (result)
                throw "DTV_GetDeviceID";

            DemodDriverInfo driverInfo;
            result = DTV_GetDriverInformation(handle_, &driverInfo);
            if (result)
                throw "DTV_GetVersion";

            printf("DriverVerion  = %s\n", driverInfo.DriverVersion);
            printf("APIVerion     = %s\n", driverInfo.APIVersion);
            printf("FWVerionLink  = %s\n", driverInfo.FWVersionLink);
            printf("FWVerionOFDM  = %s\n", driverInfo.FWVersionOFDM);
            printf("Company       = %s\n", driverInfo.Company);
            printf("SupportHWInfo = %s\n", driverInfo.SupportHWInfo);
            printf("RxDeviceID    = 0x%x\n", rxDeviceID);
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
