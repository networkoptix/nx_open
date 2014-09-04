#ifndef PACIDAL_STREAM_READER_H
#define PACIDAL_STREAM_READER_H

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
        StreamReader( CameraManager* cameraManager, int encoderNumber );
        virtual ~StreamReader();

        //!Implementation nxcip::StreamReader::getNextData
        virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
        //!Implementation nxcip::StreamReader::interrupt
        virtual void interrupt() override;

    private:
        CameraManager* m_cameraManager;
        int m_encoderNumber;
    };
}

#endif //PACIDAL_STREAM_READER_H
