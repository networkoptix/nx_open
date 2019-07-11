#pragma once

#include <memory>

#include <core/dataconsumer/abstract_data_receptor.h>
#include <common/common_module_aware.h>
#include <nx/analytics/metadata_logger.h>

namespace nx::analytics::db {

class AbstractEventsStorage;

class AnalyticsEventsReceptor:
    public QnAbstractDataReceptor,
    public QnCommonModuleAware
{
public:
    AnalyticsEventsReceptor(
        QnCommonModule* commonModule, AbstractEventsStorage* eventsStorage);

    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    AbstractEventsStorage* m_eventsStorage;
    std::unique_ptr<MetadataLogger> m_metadataLogger;
};

} // namespace nx::analytics::db
