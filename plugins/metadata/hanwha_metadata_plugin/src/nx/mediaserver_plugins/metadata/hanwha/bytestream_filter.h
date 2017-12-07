#pragma once

#if defined(ENABLED_HANWHA)

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "metadata_monitor.h"
#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

class BytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    using Handler = std::function<void(const EventList&)>;

    BytestreamFilter(const Hanwha::DriverManifest& manifest, Handler handler);
    virtual ~BytestreamFilter();
    virtual bool processData(const QnByteArrayConstRef& notification) override;

private:
    std::vector<Event> parseMetadataState(const QnByteArrayConstRef& buffer) const;

    boost::optional<Event> createEvent(
        const QString& eventSource, const QString& eventSourceState) const;

    static boost::optional<int> eventChannel(const QString& eventSource);
    static boost::optional<int> eventRegion(const QString& eventSource);
    static bool isEventActive(const QString& eventSourceState);

    static Hanwha::EventItemType eventItemType(
        const QString& eventSource, const QString& eventState);

private:
    const Hanwha::DriverManifest m_manifest;
    Handler m_handler;
};

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

#endif // defined(ENABLED_HANWHA)
