/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <memory>

#include "ilp_video_packet.h"
#include "ilp_empty_packet.h"
#include "motion_data_picture.h"
#include <iostream>

#define GENERATE_RANDOM_MOTION
#ifdef GENERATE_RANDOM_MOTION
static const unsigned int MOTION_PRESENCE_CHANCE_PERCENT = 70;
#endif


StreamReader::StreamReader(nxpt::CommonRefManager* const parentRefManager, bool isPrimary)
:
    m_refManager( parentRefManager ),
    m_isPrimary( isPrimary ),
    m_initialized(false),
    m_codec(nxcip::CODEC_ID_NONE)
{
}

StreamReader::~StreamReader()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    //std::cout << "ISD plugin getNextData started" << std::endl;

    //Vmux vmux;
    vmux_frame_t frame;
    vmux_stream_info_t stream_info;
    int rv = 0;

    if (!m_initialized)
    {
        int info_size = sizeof(stream_info);
        int streamId = m_isPrimary ? 0 : 1;
        rv = vmux.GetStreamInfo (streamId, &stream_info, &info_size);
        if (rv) {
            std::cout << "ISD plugin: can't get stream info" << std::endl;
            return nxcip::NX_INVALID_ENCODER_NUMBER; // error
        }
	if (stream_info.enc_type == 1)
	    m_codec = nxcip::CODEC_ID_H264;
	else if (stream_info.enc_type == ?)
	    m_codec = nxcip::CODEC_ID_MJPEG;
	else
	    return nxcip::NX_INVALID_ENCODER_NUMBER;

        rv = vmux.StartVideo (streamId);
        if (rv) {
            std::cout << "ISD plugin: can't start video" << std::endl;
            return nxcip::NX_INVALID_ENCODER_NUMBER; // error
        }
        m_initialized = true;
    }

    rv = vmux.GetFrame (&frame);
    if (rv) {
	std::cout << "Can't read video frame" << std::endl;
        return nxcip::NX_IO_ERROR; // error
    }


    if (frame.vmux_info.pic_type == 1) {
	std::cout << "I-frame pts = " << frame.vmux_info.PTS << "pic_type=" << frame.vmux_info.pic_type << std::endl;
    }
    //std::cout << "frame pts = " << frame.vmux_info.PTS << "pic_type=" << frame.vmux_info.pic_type << std::endl;

    std::auto_ptr<ILPVideoPacket> videoPacket( new ILPVideoPacket(
        0, // channel
        (int64_t(frame.vmux_info.PTS) * 1000000ll) / 90000,
        (frame.vmux_info.pic_type == 1 ? nxcip::MediaDataPacket::fKeyPacket : 0),
        0, // cseq
        m_codec)); 
    videoPacket->resizeBuffer( frame.frame_size );
    memcpy(videoPacket->data(), frame.frame_addr, frame.frame_size);

    *lpPacket = videoPacket.release();
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}
