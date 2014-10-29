/**********************************************************
* 28 oct 2014
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef AUDIO_STREAM_READER_H
#define AUDIO_STREAM_READER_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <plugins/camera_plugin.h>
#ifndef NO_ISD_AUDIO
#include <isd/amux/amux_iface.h>
#endif
#include <utils/network/aio/aioeventhandler.h>
#include <utils/network/aio/pollable.h>

#include "isd_audio_packet.h"


class AudioStreamReader
:
    public aio::AIOEventHandler<Pollable>
{
public:
    AudioStreamReader();
    ~AudioStreamReader();

    //!Be sure to call this after constructing object to see whether it is valid or not
    bool initializeIfNeeded();
    /*!
        \return id that can be used to remove receiver. This id is always greater than zero
    */
    size_t setPacketReceiver( std::function<void(const std::shared_ptr<AudioData>&)> packetReceiver );
    void removePacketReceiver( size_t id );
    nxcip::AudioFormat getAudioFormat() const;

private:
    mutable std::mutex m_mutex;
    std::map<size_t, std::function<void(const std::shared_ptr<AudioData>&)>> m_packetReceivers;
    amux_info_t m_audioInfo;
    std::unique_ptr<Amux> m_amux;
    nxcip::AudioFormat m_audioFormat;
    size_t m_prevReceiverID;
    std::unique_ptr<Pollable> m_pollable;
    bool m_initializedInitially;

    virtual void eventTriggered( Pollable* obj, aio::EventType eventType ) throw();

    bool initializeAmux();
    int readAudioData();
    void fillAudioFormat();
};

#endif  //AUDIO_STREAM_READER_H
