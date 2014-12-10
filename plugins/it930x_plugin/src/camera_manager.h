#ifndef ITE_CAMERA_MANAGER_H
#define ITE_CAMERA_MANAGER_H

#include <ctime>

#include <vector>
#include <deque>
#include <set>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "rx_device.h"

namespace ite
{
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
        :   m_packetsLost( 0 ),
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


        // for ReadThread

        DevReader * devReader() { return m_devReader.get(); }
        void setThreadObj(ReadThread * ptr) { m_threadObject = ptr; }

        std::shared_ptr<VideoPacketQueue> queue(unsigned num) const;

        // for StreamReader

        VideoPacket * nextPacket( unsigned encoderNumber );

        void openStream(unsigned encNo);
        void closeStream(unsigned encNo);

        // for DiscoveryManager

        unsigned short txID() const { return m_txID; }
        const nxcip::CameraInfo& info() const { return m_info; }

        void addRxDevice(RxDevicePtr dev);

        bool stopIfNeeds()
        {
            static const unsigned WAIT_TO_STOP_S = 30;

            if (m_waitStop)
            {
                if (! m_stopTime)
                    m_stopTime = time(NULL);

                if ((time(NULL) - m_stopTime) > WAIT_TO_STOP_S)
                    return stopStreams();
            }

            return false;
        }

    private:
        mutable std::mutex m_encMutex;
        mutable std::mutex m_reloadMutex;
        unsigned short m_txID;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders;
        std::vector<std::shared_ptr<VideoPacketQueue>> m_encQueues;
        std::set<unsigned> m_openedStreams;
        std::atomic_bool m_waitStop;
        time_t m_stopTime;

        RxDevicePtr m_rxDevice;
        std::vector<RxDevicePtr> m_supportedRxDevices;
        std::unique_ptr<DevReader> m_devReader;
        std::unique_ptr<LibAV> m_libAV;
        ReadThread * m_threadObject;
        std::thread m_readThread;
        bool m_hasThread;
        bool m_loading;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool captureAnyRxDevice();
        bool captureSameRxDevice(RxDevicePtr);
        bool captureFreeRxDevice(RxDevicePtr);

        bool initDevReader();
        void stopDevReader();

        void initEncoders();
        void stopEncoders();

        void reloadMedia();
        void tryLoad();

        bool stopStreams(bool force = false);

        void freeDevice()
        {
            if (m_rxDevice)
            {
                m_rxDevice->unlockF();
                m_rxDevice.reset();
            }
            m_waitStop = false;
            m_stopTime = 0;
        }

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum);

        typedef enum
        {
            STATE_NO_CAMERA,        // no Tx
            STATE_NO_FREQUENCY,     // frequency not set
            STATE_NO_RECEIVER,      // no Rx for Tx
            STATE_DEVICE_READY,     // got Rx for Tx
            STATE_STREAM_LOADING,   // loading data streams
            STATE_STREAM_READY,     // got data streams, no readers
            STATE_STREAM_LOST,      //
            STATE_STREAM_READING    // got readers
        } State;

        State checkState() const;

        //

        void getParamStr_RxID(std::string& s) const;
        void getParamStr_Channel(std::string& s) const;
        void getParamStr_Present(std::string& s) const;
        void getParamStr_Strength(std::string& s) const;
        void getParamStr_Quality(std::string& s) const;

        bool setParam_Channel(std::string& s);
    };
}

#endif // ITE_CAMERA_MANAGER_H
