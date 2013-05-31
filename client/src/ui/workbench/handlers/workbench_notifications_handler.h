#ifndef WORKBENCH_NOTIFICATIONS_HANDLER_H
#define WORKBENCH_NOTIFICATIONS_HANDLER_H

#include <QObject>

//#include <api/model/kvpair.h>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUint64KvPairUsageHelper;

class QnWorkbenchNotificationsHandler : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchNotificationsHandler(QObject *parent = 0);
    virtual ~QnWorkbenchNotificationsHandler();
    
    void addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void addSystemHealthEvent(QnSystemHealth::MessageType message);
    void addSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr& resource);
signals:
    void systemHealthEventAdded(QnSystemHealth::MessageType message, const QnResourcePtr& resource);
    void businessActionAdded(const QnAbstractBusinessActionPtr& businessAction);
    void cleared();

public slots:
    void clear();

private slots:
    void at_context_userChanged();

private:
    QnUint64KvPairUsageHelper* m_showBusinessEventsHelper;
};

#endif // WORKBENCH_NOTIFICATIONS_HANDLER_H
