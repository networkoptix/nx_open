#ifndef ITE_STREAM_READER_H
#define ITE_STREAM_READER_H

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"

namespace ite {
class CameraManager;
class ContentPacket;


namespace aux {
class FramesFrequencyWatcher;
}
//!
class StreamReader : public DefaultRefCounter<nxcip::StreamReader>
{
public:
    StreamReader(CameraManager * cameraManager, int encoderNumber);
    virtual ~StreamReader();

    // nxcip::PluginInterface

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

    // nxcip::StreamReader

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    virtual void interrupt() override;

    //

private:
    CameraManager * m_cameraManager;
    int m_encoderNumber;
    bool m_interrupted;

#if 1
    int64_t m_ts = 0;
#endif
    std::unique_ptr<aux::FramesFrequencyWatcher> m_freqWatcher;

    std::shared_ptr<ContentPacket> nextPacket();
    bool framesTooOften() const;
};

} // namespace ite

#endif // ITE_STREAM_READER_H
