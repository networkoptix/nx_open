// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rpi_camera.h"

#include "camera_manager.h"
#include "video_packet.h"
#include "stream_reader.h"

namespace rpi_cam
{
    TimeCorrection StreamReader::m_timeCorrect;

    StreamReader::StreamReader(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber)
    :   m_camera(camera),
        m_encoderNumber(encoderNumber),
        m_pts(0),
        m_interrupt(false)
    {
        debug_print("%s %d\n", __FUNCTION__, m_encoderNumber);
    }

    StreamReader::~StreamReader()
    {
        debug_print("%s %d\n", __FUNCTION__, m_encoderNumber);
    }

    void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_StreamReader)
        {
            addRef();
            return this;
        }

        return nullptr;
    }

    void StreamReader::interrupt()
    {
        m_interrupt = true;
    }

    int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
    {
        //debug_print("StreamReader.getNextData() %d\n", m_encoderNumber);

        uint64_t timeStamp = 0;
        unsigned rpiFlags = 0;

        while (! m_interrupt)
        {
            std::shared_ptr<RPiCamera> camera = m_camera.lock();

            if (! camera || ! camera->isOK())
            {
                debug_print("StreamReader.getNextData() %d no camera\n", m_encoderNumber);
                return nxcip::NX_OTHER_ERROR;
            }

            m_data.clear();
            if (camera->read(m_encoderNumber, m_data, timeStamp, rpiFlags))
            {
                // only I-Frames for second stream
                if (!m_encoderNumber || rpiFlags & RPiCamera::FLAG_SYNCFRAME)
                    break;
            }
        }

        if (m_interrupt)
        {
            m_data.clear();
            m_interrupt.store(false);

            *lpPacket = nullptr;
            return nxcip::NX_INTERRUPTED;
        }

        if (m_data.empty())
        {
            debug_print("StreamReader.getNextData() %d failed. Flags: %d\n", m_encoderNumber, rpiFlags);

            *lpPacket = nullptr;
            return nxcip::NX_NO_DATA;
        }

        timeStamp = m_timeCorrect.fixTime(m_pts, timeStamp);

#if 0   // debug: check timestamp correction
        if (rpiFlags & RPiCamera::FLAG_SYNCFRAME)
            debug_print("StreamReader: %d timestamp %llu\n", m_encoderNumber, timeStamp);
#endif

        unsigned flags = 0;
        if (rpiFlags & RPiCamera::FLAG_SYNCFRAME)
            flags |= nxcip::MediaDataPacket::fKeyPacket;

        *lpPacket = new VideoPacket(&m_data[0], m_data.size(), timeStamp, flags);
        return nxcip::NX_NO_ERROR;
    }
}
