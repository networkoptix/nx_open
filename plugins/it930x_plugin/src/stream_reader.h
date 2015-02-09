#ifndef ITE_STREAM_READER_H
#define ITE_STREAM_READER_H

#include <ctime>
#include <atomic>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"
#include "camera_manager.h"

namespace ite
{
    //!
    class StreamReader : public nxcip::StreamReader
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
        typedef int64_t PtsT;

        struct PtsTime
        {
            PtsT pts;
            time_t sec;

            PtsTime()
            :   pts(0)
            {}
        };

        CameraManager * m_cameraManager;
        int m_encoderNumber;

        PtsTime m_time;
        PtsT m_ptsPrev;

        uint64_t usecTime(PtsT pts);
    };
}

#endif // ITE_STREAM_READER_H
