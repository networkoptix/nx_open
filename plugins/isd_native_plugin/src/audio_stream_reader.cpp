/**********************************************************
* 28 oct 2014
* akolesnikov@networkoptix.com
***********************************************************/

#include "audio_stream_reader.h"

#include <iostream>

#include <QDateTime>

#include <utils/network/aio/aioservice.h>

#include "stream_time_sync_data.h"


static const int MS_PER_SEC = 1000;
static const int USEC_PER_MSEC = 1000;
static const int AUDIO_ENCODER_ID = 10;
static const int MAX_FRAMES_BETWEEN_TIME_RESYNC = 300;

AudioStreamReader::AudioStreamReader()
:
    m_prevReceiverID(0),
    m_initializedInitially(false),
    m_ptsMapper(MS_PER_SEC, &TimeSynchronizationData::instance()->timeSyncData, AUDIO_ENCODER_ID),
    m_framesSinceTimeResync(MAX_FRAMES_BETWEEN_TIME_RESYNC)
{
}

AudioStreamReader::~AudioStreamReader()
{
    if( m_pollable )
        aio::AIOService::instance()->removeFromWatch( m_pollable.get(), aio::etRead, true );
}

bool AudioStreamReader::initializeIfNeeded()
{
    if( m_initializedInitially )
        return true;
    if( !initializeAmux() )
        return false;
    m_initializedInitially = aio::AIOService::instance()->watchSocket( m_pollable.get(), aio::etRead, this );
    return m_initializedInitially;
}

/*!
    \return id that can be used to remove receiver
*/
size_t AudioStreamReader::setPacketReceiver( std::function<void(const std::shared_ptr<AudioData>&)> packetReceiver )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    const size_t id = ++m_prevReceiverID;
    m_packetReceivers[id] = std::move( packetReceiver );
    return id;
}

void AudioStreamReader::removePacketReceiver( size_t id )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    m_packetReceivers.erase( id );
}

nxcip::AudioFormat AudioStreamReader::getAudioFormat() const
{
    return m_audioFormat;
}

void AudioStreamReader::eventTriggered( Pollable* pollable, aio::EventType eventType ) throw()
{
    assert( m_pollable.get() == pollable );

    if( eventType == aio::etRead )
    {
        int result = readAudioData();
        if( result == nxcip::NX_NO_ERROR )
            return; //listening futher
    }

    aio::AIOService::instance()->removeFromWatch( m_pollable.get(), aio::etRead );
    //re-initializing audio
    if( !initializeAmux() )
        return;
    if( !aio::AIOService::instance()->watchSocket( m_pollable.get(), aio::etRead, this ) )
    {
        //TODO
    }
}

int AudioStreamReader::readAudioData()
{
    int audioBytesAvailable = m_amux->BytesAvail();
    if( audioBytesAvailable <= 0 )
    {
        std::cerr<<"ISD plugin: no audio bytes available\n";
        m_amux.reset();
        return nxcip::NX_IO_ERROR;
    }

    std::shared_ptr<AudioData> audioPacket( new AudioData(
        audioBytesAvailable,
        m_audioFormat.compressionType ) );
    if( audioPacket->capacity() < (size_t)audioBytesAvailable )
        return nxcip::NX_IO_ERROR;  //allocation error

    int bytesRead = m_amux->ReadAudio( (char*)audioPacket->data(), audioBytesAvailable );
    if( bytesRead <= 0 )
    {
        std::cerr<<"ISD plugin: failed to read audio packet\n";
        m_amux.reset();
        return nxcip::NX_IO_ERROR;
    }

    //std::cout<<"Read "<<bytesRead<<" bytes of audio"<<std::endl;
    audioPacket->setDataSize( bytesRead );

    const qint64 curTime = QDateTime::currentMSecsSinceEpoch();
    if( m_framesSinceTimeResync >= MAX_FRAMES_BETWEEN_TIME_RESYNC )
    {
        m_ptsMapper.updateTimeMapping( curTime, curTime * USEC_PER_MSEC );
        m_framesSinceTimeResync = 0;
    }
    ++m_framesSinceTimeResync;

    audioPacket->setTimestamp( m_ptsMapper.getTimestamp( curTime ) );

    //std::cout<<"Got audio packet. size "<<audioPacket->dataSize()<<", pts "<<audioPacket->timestamp()<<"\n";

    std::unique_lock<std::mutex> lk( m_mutex );
    for( auto val: m_packetReceivers )
        val.second( audioPacket );

    return nxcip::NX_NO_ERROR;
}

void AudioStreamReader::fillAudioFormat()
{
    switch( m_audioInfo.encoding )
    {
        case EncPCM:
            m_audioFormat.compressionType = nxcip::CODEC_ID_PCM_S16LE;
            break;
        case EncUlaw:
            m_audioFormat.compressionType = nxcip::CODEC_ID_PCM_MULAW;
            break;
        case EncAAC:
            m_audioFormat.compressionType = nxcip::CODEC_ID_AAC;
            break;
        default:
            std::cerr << "ISD plugin: unsupported audio codec: "<<m_audioInfo.encoding<<"\n";
            return;
    }

    m_audioFormat.sampleRate = m_audioInfo.sample_rate;
    m_audioFormat.bitrate = m_audioInfo.bit_rate;

    m_audioFormat.channels = 1;
    switch( m_audioFormat.compressionType )
    {
        case nxcip::CODEC_ID_AAC:
        {
            //TODO/IMPL parsing ADTS header to get sample rate
            break;
        }

        //case nxcip::CODEC_ID_PCM_S16LE:
        //case nxcip::CODEC_ID_PCM_MULAW:
        default:
            break;
    }

    //std::cout<<"Audio format: sample_rate "<<m_audioInfo.sample_rate<<", bitrate "<<m_audioInfo.bit_rate<<"\n";
}

bool AudioStreamReader::initializeAmux()
{
    m_amux.reset();

    std::unique_ptr<Amux> amux( new Amux() );
    memset( &m_audioInfo, 0, sizeof(m_audioInfo) );

    if( amux->GetInfo( &m_audioInfo ) )
    {
        std::cerr << "ISD plugin: can't get audio stream info. "<<strerror(errno)<<"\n";
        return false;
    }

    if( amux->StartAudio() )
    {
        std::cerr << "ISD plugin: can't start audio stream. "<<strerror(errno)<<"\n";
        return false;
    }

    //std::cout<<"AMUX initialized. Codec type "<<m_audioFormat.compressionType<<" fd = "<<amux->GetFD()<<"\n";

    m_amux = std::move(amux);
    m_pollable.reset( new Pollable( m_amux->GetFD() ) );

    fillAudioFormat();
    return true;
}
