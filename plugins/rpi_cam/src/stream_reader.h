#ifndef RPI_STREAM_READER_H
#define RPI_STREAM_READER_H

#include <atomic>
#include <vector>
#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"
#include "timer.h"

namespace rpi_cam
{
    class RPiCamera;

    //!
    class StreamReader : public nxcip::StreamReader
    {
        DEF_REF_COUNTER

    public:
        StreamReader(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber);
        virtual ~StreamReader();

        // nxcip::StreamReader

        virtual int getNextData(nxcip::MediaDataPacket** packet) override;
        virtual void interrupt() override;

        //

    private:
        std::shared_ptr<RPiCamera> m_camera;
        unsigned m_encoderNumber;
        std::vector<uint8_t> m_data;
        static TimeCorrection m_timeCorrect;
        uint64_t m_pts;

        std::atomic_bool m_interrupt;
        bool m_firstOne;
    };
}

#endif
