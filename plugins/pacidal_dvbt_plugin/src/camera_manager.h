#ifndef ITE_CAMERA_MANAGER_H
#define ITE_CAMERA_MANAGER_H

#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <plugins/camera_plugin.h>

#include "it930x.h"
#include "ref_counter.h"

namespace ite
{
    class It930x;
    class It930Stream;
    class MediaEncoder;
    class VideoPacket;

    struct LibAV;
    class DevReader;
    class ReadThread;

    typedef std::shared_ptr<VideoPacket> VideoPacketPtr;

    //!
    class VideoPacketQueue
    {
    public:
        typedef std::deque<VideoPacketPtr> QueueT;

        VideoPacketQueue()
        :
            m_packetsLost( 0 ),
            m_isLowQ( false )
        {}

        void push_back(VideoPacketPtr p) { m_deque.push_back( p ); }
        void pop_front() { m_deque.pop_front(); }
        void incLost() { ++m_packetsLost; }

        VideoPacketPtr front() { return m_deque.front(); }
        size_t size() const { return m_deque.size(); }
        bool empty() const { return m_deque.empty(); }
        std::mutex& mutex() { return m_mutex; }

        bool isLowQuality() const { return m_isLowQ; }
        void setLowQuality(bool value = true) { m_isLowQ = value; }

    private:
        mutable std::mutex m_mutex;
        QueueT m_deque;
        unsigned m_packetsLost;
        bool m_isLowQ;
    };

    //!
    class RxDevice
    {
    public:
        constexpr static const char * DEVICE_PATTERN = "usb-it930x";

        RxDevice(unsigned id);

        It930x * device() { return m_device.get(); }
        It930Stream * devStream() { return m_devStream.get(); }

        bool open();
        bool isOpen() const { return m_device.get(); }
        void close()
        {
            m_devStream.reset();
            m_device.reset();
        }

        bool lockF(unsigned freq);
        bool isLocked() const { return m_devStream.get(); }
        void unlockF() { m_devStream.reset(); }

        bool stats();

        void driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const;

        static unsigned dev2id(const std::string& devName);
        static unsigned str2id(const std::string& devName);
        static unsigned str2id(const char * devName)
        {
            std::string name(devName);
            return str2id(name);
        }

        static std::string id2str(unsigned id);

    private:
        unsigned m_id;
        uint8_t m_strength;
        std::unique_ptr<It930x> m_device;
        std::unique_ptr<It930Stream> m_devStream;
    };

    typedef std::shared_ptr<RxDevice> RxDevicePtr;

    //!
    class CameraManager : public nxcip::BaseCameraManager3
    {
        DEF_REF_COUNTER

    public:
        CameraManager(const nxcip::CameraInfo& info);
        virtual ~CameraManager();

        // nxcip::BaseCameraManager

        virtual int getEncoderCount( int* encoderCount ) const override;
        virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr ) override;
        virtual int getCameraInfo( nxcip::CameraInfo* info ) const override;
        virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
        virtual void setCredentials( const char* username, const char* password ) override;
        virtual int setAudioEnabled( int audioEnabled ) override;
        virtual nxcip::CameraPtzManager* getPtzManager() const override;
        virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
        virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
        virtual void getLastErrorString( char* errorString ) const override;

        // nxcip::BaseCameraManager2

        virtual int createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const override;
        virtual int find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const override;
        virtual int setMotionMask( nxcip::Picture* motionMask ) override;

        // nxcip::BaseCameraManager3

        virtual const char * getParametersDescriptionXML() const override;
        virtual int getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const override;
        virtual int setParamValue(const char * paramName, const char * value) override;

        //

        const nxcip::CameraInfo& info() const { return m_info; }
        VideoPacket * nextPacket( unsigned encoderNumber );

        std::shared_ptr<VideoPacketQueue> queue(unsigned num) const;

        void reload();
        bool hasEncoders() const { return m_hasThread; }

        void setThreadObj(ReadThread * ptr)
        {
            m_threadObject = ptr;
            m_hasThread = (ptr != nullptr);
        }

        DevReader * devReader() { return m_devReader.get(); }

        unsigned txID() const { return m_txID; }
        RxDevicePtr rxDevice() { return m_rxDevice; }
        void setRxDevice(RxDevicePtr dev) { m_rxDevice = dev; }

    private:
        unsigned m_txID;
        mutable std::mutex m_encMutex;
        mutable std::mutex m_reloadMutex;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders;
        std::vector<std::shared_ptr<VideoPacketQueue>> m_encQueues;

        RxDevicePtr m_rxDevice;
        std::unique_ptr<DevReader> m_devReader;
        std::unique_ptr<LibAV> m_libAV;
        ReadThread * m_threadObject;
        std::thread m_readThread;
        std::atomic_bool m_hasThread;
        bool m_threadJoined;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool initDevReader();
        void stopDevReader();
        bool hasDevReader() const { return m_devReader.get(); }

        void initEncoders();
        void stopEncoders();

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum);
    };
}

#endif // ITE_CAMERA_MANAGER_H
