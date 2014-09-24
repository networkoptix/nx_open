#ifndef PACIDAL_STREAM_READER_H
#define PACIDAL_STREAM_READER_H

#include <atomic>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"
#include "camera_manager.h"

namespace pacidal
{
    //!
    class StreamReader
    :
        public nxcip::StreamReader
    {
        DEF_REF_COUNTER

    public:
        StreamReader(nxpt::CommonRefManager* refManager, CameraManager* cameraManager, int encoderNumber);
        virtual ~StreamReader();

        // nxcip::StreamReader

        virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
        virtual void interrupt() override;

        //

    private:
        CameraManager* m_cameraManager;
        int m_encoderNumber;
    };
}

#endif //PACIDAL_STREAM_READER_H
