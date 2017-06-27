#ifndef WORKBENCH_NOTIFICATIONS_HANDLER_H
#define WORKBENCH_NOTIFICATIONS_HANDLER_H

#include <QtCore/QObject>

#include <api/model/kvpair.h>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <ui/workbench/workbench_state_manager.h>
#include <nx_ec/ec_api.h>

#include <utils/common/connective.h>

class QnWorkbenchUserEmailWatcher;
class QnBusinessEventsFilterResourcePropertyAdaptor;

class QnWorkbenchNotificationsHandler : public Connective<QObject>, public QnSessionAwareDelegate
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit QnWorkbenchNotificationsHandler(QObject *parent = 0);
    virtual ~QnWorkbenchNotificationsHandler();

    void addSystemHealthEvent(QnSystemHealth::MessageType message);
    void addSystemHealthEvent(QnSystemHealth::MessageType message, const QnAbstractBusinessActionPtr &businessAction);

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

signals:
    void systemHealthEventAdded( QnSystemHealth::MessageType message, const QVariant& params );
    void systemHealthEventRemoved( QnSystemHealth::MessageType message, const QVariant& params );

    void notificationAdded(const QnAbstractBusinessActionPtr& businessAction);
    void notificationRemoved(const QnAbstractBusinessActionPtr& businessAction);

    void cleared();

public slots:
    void clear();

private slots:
    void at_context_userChanged();
    void at_userEmailValidityChanged(const QnUserResourcePtr &user, bool isValid);

    void at_eventManager_connectionClosed();
    void at_eventManager_actionReceived(const QnAbstractBusinessActionPtr& businessAction);

    void at_settings_valueChanged(int id);
    void at_emailSettingsChanged();

private:
    void addNotification(const QnAbstractBusinessActionPtr& businessAction);

    /**
     * Check that system health message can be displayed to admins only.
     */
    bool adminOnlyMessage(QnSystemHealth::MessageType message);

    void setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible);
    void setSystemHealthEventVisible(QnSystemHealth::MessageType message, const QnResourcePtr& resource, bool visible);

    void setSystemHealthEventVisibleInternal(QnSystemHealth::MessageType message, const QVariant& params, bool visible);

    void checkAndAddSystemHealthMessage(QnSystemHealth::MessageType message);

private:
    QnWorkbenchUserEmailWatcher *m_userEmailWatcher;
    QnBusinessEventsFilterResourcePropertyAdaptor *m_adaptor;
    quint64 m_popupSystemHealthFilter;
};

#endif // WORKBENCH_NOTIFICATIONS_HANDLER_H
