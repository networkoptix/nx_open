#ifndef POPUP_COLLECTION_WIDGET_H
#define POPUP_COLLECTION_WIDGET_H

#include <QWidget>

#include <api/model/kvpair.h>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <health/system_health.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupCollectionWidget;
}

class QnBusinessEventPopupWidget;
class QnSystemHealthPopupWidget;
class QnUint64KvPairUsageHelper;

class QnPopupCollectionWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QWidget base_type;
    
public:
    explicit QnPopupCollectionWidget(QnWorkbenchContext* context);
    explicit QnPopupCollectionWidget(QWidget *parent);
    virtual ~QnPopupCollectionWidget();

    void init();

    bool addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    bool addSystemHealthEvent(QnSystemHealth::MessageType message);
    bool addSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourceList &resources);

    bool isEmpty() const;

    void clear(bool animate = true);
private slots:
    void at_businessEventWidget_closed(BusinessEventType::Value actionType, bool ignore);
    void at_systemHealthWidget_closed(QnSystemHealth::MessageType message, bool ignore);

    void at_postponeAllButton_clicked();
    void at_minimizeButton_clicked();

    void at_context_userChanged();
private:
    QnUint64KvPairUsageHelper* m_showBusinessEventsHelper;

    QScopedPointer<Ui::QnPopupCollectionWidget> ui;

    QMap<BusinessEventType::Value, QnBusinessEventPopupWidget *> m_businessEventWidgets;
    QMap<QnSystemHealth::MessageType, QnSystemHealthPopupWidget *> m_systemHealthWidgets;
};

#endif // POPUP_COLLECTION_WIDGET_H
