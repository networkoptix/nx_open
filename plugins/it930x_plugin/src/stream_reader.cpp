#include <memory>

#include "discovery_manager.h"
#include "camera_manager.h"
#include "video_packet.h"
#include "dev_reader.h"
#include "stream_reader.h"

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

    bool isKey(ite::ContentPacketPtr packet)
    {
        try
        {
            auto type = FrameTypeExtractor::getH264FrameType(packet->data(), packet->size());
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

            if (type == FrameTypeExtractor::I_Frame)
                printf("%s (size %ld; pts %ld; time %ld)\n", strFrameType, packet->size(), packet->pts(), time(NULL));
#endif
            return type == FrameTypeExtractor::I_Frame;
        }
        catch (...)
        {
#if 1
            printf("exception in getH264FrameType()\n");
#endif
        }

        return false;
    }
}

namespace ite
{
    INIT_OBJECT_COUNTER(StreamReader)
    DEFAULT_REF_COUNTER(StreamReader)

    StreamReader::StreamReader(CameraManager * cameraManager, int encoderNumber)
    :   m_refManager(this),
        m_cameraManager(cameraManager),
        m_encoderNumber(encoderNumber),
        m_interrupted(false)
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
        ContentPacketPtr pkt = nextPacket();
        if (!pkt || pkt->empty())
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

        bool key = isKey(pkt);
        uint64_t timestamp = m_ptsTime.pts2usec(pkt->pts());
#if 0
        if (key && !m_encoderNumber)
            printf("pts: %u; time: %lu; stamp: %lu\n", pkt->pts(), time(NULL), timestamp / 1000 / 1000);
#endif

        VideoPacket * packet = new VideoPacket(pkt->data(), pkt->size(), timestamp);
        if (!packet)
        {
            *lpPacket = nullptr;
            return nxcip::NX_TRY_AGAIN;
        }

        if (key)
            packet->setFlag(nxcip::MediaDataPacket::fKeyPacket);

        if (pkt->flags() & ContentPacket::F_StreamReset)
            packet->setFlag(nxcip::MediaDataPacket::fStreamReset);

        if (m_encoderNumber)
            packet->setFlag(nxcip::MediaDataPacket::fLowQuality);

        *lpPacket = packet;
        return nxcip::NX_NO_ERROR;
    }

    void StreamReader::interrupt()
    {
        DevReader * devReader = m_cameraManager->devReader();
        if (devReader)
        {
            m_interrupted = true;
            devReader->interrupt();
        }
    }

    //

    ContentPacketPtr StreamReader::nextPacket()
    {
        static const unsigned TIMEOUT_MS = 10000;
        std::chrono::milliseconds timeout(TIMEOUT_MS);

        Timer timer(true);
        while (timer.elapsedMS() < TIMEOUT_MS)
        {
            DevReader * devReader = m_cameraManager->devReader();
            if (! devReader)
                break;

            ContentPacketPtr pkt = devReader->getPacket(RxDevice::stream2pid(m_encoderNumber), timeout);
            if (m_interrupted)
                break;

            if (pkt)
            {
#if 1
                if (pkt->errors())
                    printf("TS errors: %d\n", pkt->errors());
#endif
                return pkt;
            }
        }

        m_interrupted = false;
        return nullptr;
    }
}
