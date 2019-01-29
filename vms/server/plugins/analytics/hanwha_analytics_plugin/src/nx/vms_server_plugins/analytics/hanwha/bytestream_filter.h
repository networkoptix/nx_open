#pragma once

#include <boost/optional/optional.hpp>

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

class BytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    using Handler = std::function<void(const EventList&)>;

    BytestreamFilter(const Hanwha::EngineManifest& manifest, Handler handler);
    virtual ~BytestreamFilter() = default;
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
    const Hanwha::EngineManifest m_manifest;
    Handler m_handler;
};

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
