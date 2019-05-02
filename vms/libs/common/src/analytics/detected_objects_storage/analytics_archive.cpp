#include "analytics_archive.h"

namespace nx::analytics::storage {

AnalyticsArchive::AnalyticsArchive(
    const QString& dataDir,
    const QnUuid& resourceId)
    :
    m_dataDir(dataDir),
    m_resourceId(resourceId)
{
}

AnalyticsArchive::~AnalyticsArchive()
{
}

bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds /*timestamp*/,
    const std::vector<QRectF>& /*region*/,
    int /*objectType*/,
    long long /*allAttributesHash*/)
{
    // TODO
    return false;
}

QnTimePeriodList AnalyticsArchive::matchPeriod(const Filter& /*filter*/)
{
    // TODO
    return {};
}

} // namespace nx::analytics::storage
