#pragma once

#include <core/dataconsumer/abstract_data_receptor.h>

namespace nx::analytics::db {

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

} // namespace nx::analytics::db
