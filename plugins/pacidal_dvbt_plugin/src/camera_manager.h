#ifndef ITE_CAMERA_MANAGER_H
#define ITE_CAMERA_MANAGER_H

#include <vector>
#include <deque>
#include <set>
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
    class CameraManager;

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

        unsigned rxID() const { return m_id; }

        It930x * device() { return m_device.get(); }
        It930Stream * devStream() { return m_devStream.get(); }

        bool open();
        bool isOpen() const { return m_device.get(); }
        void close()
        {
            m_devStream.reset();
            m_device.reset();
        }

        bool lockF(CameraManager * cam, unsigned freq);
        bool isLocked() const { return m_devStream.get(); }
        void unlockF()
        {
            m_devStream.reset();
            m_frequency = 0;
            m_camera = nullptr;
        }

        bool stats();

        void driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const;

        std::mutex& mutex() { return m_mutex; }

        unsigned frequency() const { return m_frequency; }
        uint8_t strength() const { return m_strength; }
        bool presented() const { return m_presented; }

        static unsigned dev2id(const std::string& devName);
        static unsigned str2id(const std::string& devName);
        static unsigned str2id(const char * devName)
        {
            std::string name(devName);
            return str2id(name);
        }

        static std::string id2str(unsigned id);

        CameraManager * camera() { return m_camera; }

    private:
        unsigned m_id;
        std::unique_ptr<It930x> m_device;
        std::unique_ptr<It930Stream> m_devStream;
        mutable std::mutex m_mutex;
        CameraManager * m_camera;
        unsigned m_frequency;
        uint8_t m_strength;
        bool m_presented;
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

        unsigned frequency() const { return m_frequency; }
        void setFrequency(unsigned freq) { m_frequency = freq; }
        void addRxDevice(RxDevicePtr dev);

        void openStream(unsigned encNo);
        void closeStream(unsigned encNo);
        bool stopStreams();

    private:
        unsigned m_txID;
        unsigned m_frequency;
        mutable std::mutex m_encMutex;
        mutable std::mutex m_reloadMutex;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders;
        std::vector<std::shared_ptr<VideoPacketQueue>> m_encQueues;
        std::set<unsigned> m_openedStreams;

        RxDevicePtr m_rxDevice;
        std::vector<RxDevicePtr> m_supportedRxDevices;
        std::unique_ptr<DevReader> m_devReader;
        std::unique_ptr<LibAV> m_libAV;
        ReadThread * m_threadObject;
        std::thread m_readThread;
        std::atomic_bool m_hasThread;
        bool m_threadJoined;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool captureAnyRxDevice();
        bool captureSameRxDevice();
        bool captureFreeRxDevice(RxDevicePtr);
        bool captureOpenedRxDevice(RxDevicePtr);

        bool initDevReader();
        void stopDevReader();
        bool hasDevReader() const { return m_devReader.get(); }

        void initEncoders();
        void stopEncoders();

        void getParamStr_Frequency(std::string& s) const;
        void getParamStr_Presented(std::string& s) const;
        void getParamStr_Strength(std::string& s) const;

        void setParam_Frequency(std::string& s);

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum);
    };
}

#endif // ITE_CAMERA_MANAGER_H
