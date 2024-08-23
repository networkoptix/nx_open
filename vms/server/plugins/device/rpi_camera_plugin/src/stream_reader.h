// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef RPI_STREAM_READER_H
#define RPI_STREAM_READER_H

#include <atomic>
#include <memory>
#include <vector>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"
#include "timer.h"

namespace rpi_cam
{
    class RPiCamera;

    //!
    class StreamReader : public DefaultRefCounter<nxcip::StreamReader>
    {
    public:
        StreamReader(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber);
        virtual ~StreamReader();

        // nxpl::PluginInterface

        virtual void * queryInterface( const nxpl::NX_GUID& interfaceID ) override;

        // nxcip::StreamReader

        virtual int getNextData(nxcip::MediaDataPacket** packet) override;
        virtual void interrupt() override;

        //

    private:
        std::weak_ptr<RPiCamera> m_camera;
        unsigned m_encoderNumber;
        std::vector<uint8_t> m_data;
        static TimeCorrection m_timeCorrect;
        uint64_t m_pts;

        std::atomic_bool m_interrupt;
    };
}

#endif
