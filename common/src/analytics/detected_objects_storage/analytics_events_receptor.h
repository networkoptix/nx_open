#pragma once

#include <core/dataconsumer/abstract_data_receptor.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

class AbstractEventsStorage;

class AnalyticsEventsReceptor:
    public QnAbstractDataReceptor
{
public:
    AnalyticsEventsReceptor(AbstractEventsStorage* eventsStorage);

    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    AbstractEventsStorage* m_eventsStorage;
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
