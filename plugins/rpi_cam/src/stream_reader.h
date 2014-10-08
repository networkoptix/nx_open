#ifndef RPI_STREAM_READER_H
#define RPI_STREAM_READER_H

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"

namespace rpi_cam
{
    class MediaEncoder;

    //!
    class StreamReader : public nxcip::StreamReader
    {
        DEF_REF_COUNTER

    public:
        StreamReader(MediaEncoder * encoder);
        virtual ~StreamReader();

        // nxcip::StreamReader

        virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
        virtual void interrupt() override;

        //

    private:
        MediaEncoder * encoder_;
    };
}

#endif
