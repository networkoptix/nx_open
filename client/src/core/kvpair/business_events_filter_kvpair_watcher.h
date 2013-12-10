#ifndef BUSINESS_EVENTS_FILTER_KVPAIR_WATCHER_H
#define BUSINESS_EVENTS_FILTER_KVPAIR_WATCHER_H

#include <business/business_fwd.h>

#include <core/kvpair/abstract_kvpair_watcher.h>

class QnBusinessEventsFilterKvPairWatcher : public QnAbstractKvPairWatcher
{
    Q_OBJECT

    typedef QnAbstractKvPairWatcher base_type;
public:
    explicit QnBusinessEventsFilterKvPairWatcher(QObject *parent = 0);
    virtual ~QnBusinessEventsFilterKvPairWatcher();

    virtual QString key() const override;

    virtual void updateValue(int resourceId, const QString &value) override;

    virtual void removeValue(int resourceId) override;

    bool eventAllowed(int resourceId, BusinessEventType::Value eventType) const;

    quint64 combinedValue(int resourceId) const;
    void setCombinedValue(int resourceId, quint64 value);

    static quint64 defaultCombinedValue();
signals:
    void valueChanged(int resourceId, quint64 combinedValue);

private:
    QHash<int, quint64> m_valueByResourceId;
};

#endif // BUSINESS_EVENTS_FILTER_KVPAIR_WATCHER_H
