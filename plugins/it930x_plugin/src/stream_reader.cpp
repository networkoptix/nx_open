#include <ctime>
#include <memory>
#include <utility>

#include "discovery_manager.h"
#include "stream_reader.h"
#include "video_packet.h"

#if 1
#include "../../../common/src/utils/media/nalUnits.h"
#include "../../../common/src/utils/media/bitStream.h"
#endif


namespace
{
    ///
    class FrameTypeExtractor
    {
    public:
        static constexpr bool dataWithNalPrefixes() { return true; }

        enum FrameType
        {
            UnknownFrameType,
            I_Frame,
            P_Frame,
            B_Frame
        };

        static FrameTypeExtractor::FrameType getH264FrameType(const quint8 * data, int size)
        {
            if (size < 4)
                return UnknownFrameType;

            const quint8* end = data + size;
            while (data < end)
            {
                if (dataWithNalPrefixes())
                {
                    data = NALUnit::findNextNAL(data, end);
                    if (data >= end)
                        break;
                }
                else {
                    data += 4;
                }

                quint8 nalType = *data & 0x1f;
                if (nalType >= nuSliceNonIDR && nalType <= nuSliceIDR)
                {
                    if (nalType == nuSliceIDR)
                        return I_Frame;
                    quint8 nal_ref_idc = (*data >> 5) & 3;
                    if (nal_ref_idc)
                        return P_Frame;

                    BitStreamReader bitReader;
                    bitReader.setBuffer(data+1, end);
                    try {
                        /*int first_mb_in_slice =*/ NALUnit::extractUEGolombCode(bitReader);

                        int slice_type = NALUnit::extractUEGolombCode(bitReader);
                        if (slice_type >= 5)
                            slice_type -= 5; // +5 flag is: all other slice at this picture must be same type

                        if (slice_type == SliceUnit::I_TYPE || slice_type == SliceUnit::SI_TYPE)
                            return P_Frame; // fake. It is i-frame, but not IDR, so we can't seek to this time and e.t.c.  // I_Frame;
                        else if (slice_type == SliceUnit::P_TYPE || slice_type == SliceUnit::SP_TYPE)
                            return P_Frame;
                        else if (slice_type == SliceUnit::B_TYPE)
                            return B_Frame;
                        else
                            return UnknownFrameType;
                    } catch(...) {
                        return UnknownFrameType;
                    }
                }
                if (! dataWithNalPrefixes())
                    break;
            }
            return UnknownFrameType;
        }
    };
}

namespace ite
{
#if 1
    DEFAULT_REF_COUNTER(StreamReader)
#else
    unsigned int StreamReader::addRef()
    {
        unsigned count = m_refManager.addRef();
        return count;
    }

    unsigned int StreamReader::releaseRef()
    {
        unsigned count = m_refManager.releaseRef();
        return count;
    }
#endif

    StreamReader::StreamReader(CameraManager * cameraManager, int encoderNumber)
    :
        m_refManager( this ),
        m_cameraManager( cameraManager ),
        m_encoderNumber( encoderNumber )
    {
        m_cameraManager->openStream(m_encoderNumber);
    }

    StreamReader::~StreamReader()
    {
        m_cameraManager->closeStream(m_encoderNumber);
    }

    void * StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_StreamReader)
        {
            addRef();
            return this;
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
    {
        VideoPacket * packet = m_cameraManager->nextPacket( m_encoderNumber );
        if (!packet)
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

        auto type = FrameTypeExtractor::getH264FrameType((const uint8_t *)packet->data(), packet->dataSize());
        if (type == FrameTypeExtractor::I_Frame)
            packet->setKeyFlag();
#if 0
        const char * strFrameType = "Unknown";
        switch(type)
        {
            case FrameTypeExtractor::I_Frame:
                strFrameType = "I-frame";
                break;

            case FrameTypeExtractor::P_Frame:
                strFrameType = "P-frame";
                break;

            case FrameTypeExtractor::B_Frame:
                strFrameType = "B-frame";
                break;

            default:
                break;
        }

        printf("%s cam: %d enc: %d (size %d; pts %ld; time %ld)\n", strFrameType, m_cameraManager->txID(), m_encoderNumber, packet->dataSize(), packet->pts(), time(NULL));
#endif
        packet->setTime( usecTime(packet->pts()) );
        *lpPacket = packet;
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {
    }

    //

    // TODO: PTS overflow
    uint64_t StreamReader::usecTime(PtsT pts)
    {
        static const unsigned USEC_IN_SEC = 1000 * 1000;
        static const unsigned MAX_PTS_DRIFT = 63000;    // 700 ms
        static const double timeBase = 1.0 / 9000;

        PtsT drift = pts - m_ptsPrev;
        if (drift < 0) // abs
            drift = -drift;

        if (!m_ptsPrev || drift > MAX_PTS_DRIFT)
        {
            m_time.pts = pts;
            m_time.sec = time(NULL);
        }

        m_ptsPrev = pts;
        return (m_time.sec + (pts - m_time.pts) * timeBase) * USEC_IN_SEC;
    }
}
