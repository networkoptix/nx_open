#ifndef PACIDAL_CAMERA_MANAGER_H
#define PACIDAL_CAMERA_MANAGER_H

#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"

namespace pacidal
{
    class PacidalIt930x;
    class PacidalStream;
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
    class CameraManager
    :
        public nxcip::BaseCameraManager2
    {
        DEF_REF_COUNTER

    public:
        constexpr static const char* DEVICE_PATTERN = "usb-it930x";

        CameraManager(const nxcip::CameraInfo& info, bool init = true);
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

        //

        const nxcip::CameraInfo& info() const { return m_info; }
        VideoPacket* nextPacket( unsigned encoderNumber );

        static unsigned cameraId( const nxcip::CameraInfo& info );

        void fillInfo(nxcip::CameraInfo& info) const;

        std::shared_ptr<VideoPacketQueue> queue(unsigned num) const;

        void reload();

        bool hasEncoders() const { return m_hasThread; }

        void setThreadObj(ReadThread * ptr)
        {
            m_threadObject = ptr;
            m_hasThread = (ptr != nullptr);
        }

        DevReader * devReader() { return m_devReader.get(); }

    private:
        mutable std::mutex m_encMutex;
        mutable std::mutex m_reloadMutex;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders;
        std::vector<std::shared_ptr<VideoPacketQueue>> m_encQueues;

        std::unique_ptr<PacidalIt930x> m_device;
        std::unique_ptr<PacidalStream> m_devStream;
        std::unique_ptr<DevReader> m_devReader;
        std::unique_ptr<LibAV> m_libAV;
        ReadThread * m_threadObject;
        std::thread m_readThread;
        std::atomic_bool m_hasThread;
        bool m_threadJoined;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool initDevice(unsigned id);
        void stopDevice();
        bool devStats() const;

        bool initDevStream();
        void stopDevStream();
        bool hasDevStream() const { return m_devStream.get(); }

        bool initDevReader();
        void stopDevReader();
        bool hasDevReader() const { return m_devReader.get(); }

        void initEncoders();
        void stopEncoders();

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum);
        void driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const;
    };
}

#endif  //PACIDAL_CAMERA_MANAGER_H
