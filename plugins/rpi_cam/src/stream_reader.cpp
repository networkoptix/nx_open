#include "rpi_camera.h"

#include "camera_manager.h"
#include "video_packet.h"
#include "stream_reader.h"

namespace rpi_cam
{
    DEFAULT_REF_COUNTER(StreamReader)

    TimeCorrection StreamReader::m_timeCorrect;

    StreamReader::StreamReader(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber)
    :   m_refManager(this),
        m_camera(camera),
        m_encoderNumber(encoderNumber),
        m_pts(0),
        m_interrupt(false),
        m_firstOne(true)
    {
        printf("[camera] StreamReader() %d\n", m_encoderNumber);
    }

    StreamReader::~StreamReader()
    {
        printf("[camera] ~StreamReader() %d\n", m_encoderNumber);
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
        //printf("[camera] StreamReader.getNextData() %d\n", m_encoderNumber);

        uint64_t timeStamp = 0;
        unsigned rpiFlags = 0;

        while (! m_interrupt)
        {
            if (! m_camera || ! m_camera->isOK())
            {
                printf("[camera] StreamReader.getNextData() %d no camera\n", m_encoderNumber);
                return false;
            }

            if (m_firstOne)
            {
                m_camera->fillBuffer(m_encoderNumber);
                m_firstOne = false;
            }

            m_data.clear();
            if (m_camera->read(m_encoderNumber, m_data, timeStamp, rpiFlags))
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
            printf("[camera] StreamReader.getNextData() %d failed. Flags: %d\n", m_encoderNumber, rpiFlags);

            *lpPacket = nullptr;
            return nxcip::NX_NO_DATA;
        }

        timeStamp = m_timeCorrect.fixTime(m_pts, timeStamp);
#if 0
        if (rpiFlags & RPiCamera::FLAG_SYNCFRAME)
            printf("[camera] StreamReader: %d timestamp %llu\n", m_encoderNumber, timeStamp);
#endif

        unsigned flags = 0;
        if (rpiFlags & RPiCamera::FLAG_SYNCFRAME)
            flags |= nxcip::MediaDataPacket::fKeyPacket;

        *lpPacket = new VideoPacket(&m_data[0], m_data.size(), timeStamp, flags);
        return nxcip::NX_NO_ERROR;
    }
}
