#pragma once

#include <boost/optional/optional.hpp>

#include <hanwha_metadata_monitor.h>
#include <hanwha_common.h>

#include <plugins/plugin_api.h>
#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaMetadataMonitor;

class HanwhaBytestreamFilter: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    using Handler = std::function<void(const HanwhaEventList&)>;

    HanwhaBytestreamFilter(Handler handler);
    virtual ~HanwhaBytestreamFilter();
    virtual bool processData(const QnByteArrayConstRef& notification) override;

private:
    std::vector<HanwhaEvent> parseMetadataState(const QnByteArrayConstRef& buffer) const;
    boost::optional<HanwhaEvent> createEvent(const QString& eventSource, const QString& eventSourceState) const;

    boost::optional<nxpl::NX_GUID> eventTypeId(const QString& eventSource) const;
    boost::optional<int> eventChannel(const QString& eventSource) const;
    boost::optional<int> eventRegion(const QString& eventSource) const;
    bool isEventActive(const QString& eventSourceState) const;
    HanwhaEventItemType eventItemType(const QString& eventSource, const QString& eventState) const;
    
    void fillCaption(HanwhaEvent* inOutEvent) const;
    void fillDescription(HanwhaEvent* inOutEvent) const;

private:
    Handler m_handler;

};

} // namespace plugins
} // namespace mediaserver
} // namespace nx