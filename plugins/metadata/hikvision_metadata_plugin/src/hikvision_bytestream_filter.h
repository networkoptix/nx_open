#pragma once

#include <boost/optional/optional.hpp>

#include <hikvision_metadata_monitor.h>
#include <hikvision_common.h>

#include <plugins/plugin_api.h>
#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HikvisionMetadataMonitor;

class HikvisionBytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    using Handler = std::function<void(const HikvisionEventList&)>;

    HikvisionBytestreamFilter(const Hikvision::DriverManifest& manifest, Handler handler);
    virtual ~HikvisionBytestreamFilter();
    virtual bool processData(const QnByteArrayConstRef& notification) override;
private:
    void addExpiredEvents(std::vector<HikvisionEvent>& result);
private:
    const Hikvision::DriverManifest m_manifest;
    Handler m_handler;

    struct StartedEvent
    {
        StartedEvent(const HikvisionEvent& event = HikvisionEvent()):
            event(event)
        {
            timer.restart();
        }

        HikvisionEvent event;
        nx::utils::ElapsedTimer timer;
    };

    QMap<QString, StartedEvent> m_startedEvents;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
