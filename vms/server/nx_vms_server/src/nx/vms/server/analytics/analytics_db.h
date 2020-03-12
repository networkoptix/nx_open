#pragma once

#include <nx/analytics/db/analytics_db.h>

class QnMediaServerModule;

namespace nx::vms::server::analytics {

class AnalyticsDb:
    public nx::analytics::db::EventsStorage
{
    using base_type = nx::analytics::db::EventsStorage;

public:
    AnalyticsDb(
        QnMediaServerModule* mediaServerModule,
        nx::analytics::db::AbstractIframeSearchHelper* iframeSearchHelper);

    virtual std::vector<nx::analytics::db::ObjectPosition>
        lookupTrackDetailsSync(const nx::analytics::db::ObjectTrack& track) override;

protected:
    virtual bool makePath(const QString& path) override;
    virtual bool makeWritable(const std::vector<PathAndMode>& pathAndModeList) override;

private:
    QnMediaServerModule* m_mediaServerModule = nullptr;
};

} // namespace nx::vms::server::analytics
