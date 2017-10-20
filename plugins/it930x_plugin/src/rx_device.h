#ifndef ITE_RX_DEVICE_H
#define ITE_RX_DEVICE_H

#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "it930x.h"
#include "dev_reader.h"
#include <plugins/camera_plugin.h>

namespace nxcip
{
    struct LiveStreamConfig;
}

namespace ite
{
    class CameraManager;

    unsigned str2num(std::string& s);
    std::string num2str(unsigned id);

    class TxDevice;
    typedef std::shared_ptr<TxDevice> TxDevicePtr;

    class RxDevice
    {
    public:
        constexpr static const char * DEVICE_PATTERN = "usb-it930x";

        enum
        {
            NOT_A_CHANNEL = 16 // TxDevice::CHANNELS_NUM
        };

        static constexpr unsigned MIN_QUALITY() { return 60; }

        static constexpr unsigned RC_DELAY_MS() { return 250; }
        static constexpr unsigned RC_ATTEMPTS() { return 16; }

        static uint16_t stream2pid(unsigned stream)
        {
            switch (stream)
            {
                case 0: return Pacidal::PID_VIDEO_FHD;
                case 1: return Pacidal::PID_VIDEO_SD;
                default:
                    return Pacidal::PID_VIDEO_FHD;
            }
        }
#if 0
        static unsigned pid2stream(uint16_t pid)
        {
            switch (pid)
            {
                case Pacidal::PID_VIDEO_FHD: return 0;
                case Pacidal::PID_VIDEO_SD: return 1;
                default:
                    return 0;
            }
        }
#endif
        static constexpr unsigned streamsCountPassive() { return 2; }
        unsigned streamsCount();

        static void resolutionPassive(unsigned stream, int& width, int& height, float& fps);
        void resolution(unsigned stream, int& width, int& height, float& fps);

        //

        RxDevice(unsigned id);
        ~RxDevice();

        unsigned short rxID() const { return m_rxID; }

        bool isOpened() const { return m_it930x.get(); }
        DevReader * reader() { return m_devReader.get(); }
        bool isReading() const { return m_devReader->hasThread(); }

        bool subscribe(unsigned stream) { return m_devReader->subscribe(stream2pid(stream)); }
        void unsubscribe(unsigned stream) { m_devReader->unsubscribe(stream2pid(stream)); }

        bool configureTx();
        void processRcQueue();
        void updateTxParams();

        const IteDriverInfo rxDriverInfo() const { return m_rxInfo; }
        TxDevicePtr txDevice() const { return m_txDev; }

        unsigned channel() { return m_channel; }
        uint8_t strength() const { return m_signalStrength; }
        uint8_t quality() const { return m_signalQuality; }
        bool present() const { return m_signalPresent; }
        bool good() const;

        static unsigned dev2id(const std::string& devName);
        static unsigned str2id(const std::string& devName);
        static unsigned str2id(const char * devName)
        {
            std::string name(devName);
            return str2id(name);
        }

        static std::string id2str(unsigned id);

        // Encoder stuff

        bool getEncoderParams(unsigned streamNo, nxcip::LiveStreamConfig& config);
        bool setEncoderParams(unsigned streamNo, const nxcip::LiveStreamConfig& config);

        // TX stuff
        bool changeChannel(unsigned channel);
        void updateOSD();
        void stats();

        // <refactor>
        bool findBestTx();

        bool testChannel(int chan, int &bestChan, int &bestStrength, int &txID);

        bool deviceReady() const {return m_deviceReady;}
        nxcip::CameraInfo getCameraInfo() const;
        TxDevicePtr getTxDevice() const {return m_txDev;}
        void startReader()
        {
            if (!m_devReader->hasThread())
                m_devReader->start(m_it930x.get());
        }

        void stopReader()
        {
            m_devReader->stop();
            m_devReader->clearBuf();
        }

        void shutdown();
        void startWatchDog();

        void setCamera(CameraManager *cam)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cam = cam;
        }
        bool isLocked() const;

        void pleaseStop() { m_needStop = true; }

        bool needStop() const { return m_needStop; }
        bool running() const { return m_running; }

    private:
        std::atomic_bool 	m_deviceReady;
        // </refactor>

    private:

        mutable std::mutex m_mutex;
        mutable std::mutex m_cvMutex;
        std::condition_variable m_cvConfig;
        std::atomic<bool> m_needStop;
        std::atomic<bool> m_running;
        nx::utils::thread m_watchDogThread;
        bool m_configuring;

        std::unique_ptr<It930x> m_it930x;
        std::unique_ptr<DevReader> m_devReader;
        TxDevicePtr m_txDev;                    // locked Tx device (as camera)
        const uint16_t m_rxID;
        std::atomic<uint8_t> m_channel;

        // info from DTV receiver
        uint8_t m_signalQuality;
        uint8_t m_signalStrength;
        bool m_signalPresent;
        IteDriverInfo m_rxInfo;

        CameraManager *m_cam;
        int m_minStrength = 70;
        // locked Tx device

        bool isLocked_u() const { return m_it930x.get() && m_it930x->hasStream(); }
        bool wantedByCamera() const;
        void resetFrozen();
        bool open();
        void close();
        bool lockAndStart(int chan, int txId);

        bool sendRC(RcCommand * cmd);
        bool sendRC(RcCommand * cmd, unsigned attempts);
    };

    typedef std::shared_ptr<RxDevice> RxDevicePtr;
}

#endif
