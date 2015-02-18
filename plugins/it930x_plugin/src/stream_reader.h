#ifndef ITE_STREAM_READER_H
#define ITE_STREAM_READER_H

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"
#include "timer.h"

namespace ite
{
    class CameraManager;
    class ContentPacket;

    //!
    class StreamReader : public nxcip::StreamReader, public ObjectCounter<StreamReader>
    {
        DEF_REF_COUNTER

    public:
        StreamReader(CameraManager * cameraManager, int encoderNumber);
        virtual ~StreamReader();

        // nxcip::StreamReader

        virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
        virtual void interrupt() override;

        //

    private:
        CameraManager * m_cameraManager;
        int m_encoderNumber;
        bool m_interrupted;
        PtsTime m_ptsTime;

        std::shared_ptr<ContentPacket> nextPacket();
    };
}

#endif // ITE_STREAM_READER_H
