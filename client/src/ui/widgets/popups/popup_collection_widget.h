#ifndef POPUP_COLLECTION_WIDGET_H
#define POPUP_COLLECTION_WIDGET_H

#include <QWidget>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <health/system_health.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupCollectionWidget;
}

class QnBusinessEventPopupWidget;
class QnSystemHealthPopupWidget;

class QnPopupCollectionWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QWidget base_type;
    
public:
    explicit QnPopupCollectionWidget(QWidget *parent, QnWorkbenchContext *context = NULL);
    virtual ~QnPopupCollectionWidget();

    bool addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    bool addSystemHealthEvent(QnSystemHealth::MessageType message);
    bool addSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourceList &resources);

    bool isEmpty() const;

    void clear();
protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;

private slots:
    void updatePosition();

    void at_businessEventWidget_closed(BusinessEventType::Value actionType, bool ignore);
    void at_systemHealthWidget_closed(QnSystemHealth::MessageType message, bool ignore);
private:
    QScopedPointer<Ui::QnPopupCollectionWidget> ui;

    QMap<BusinessEventType::Value, QnBusinessEventPopupWidget *> m_businessEventWidgets;
    QMap<QnSystemHealth::MessageType, QnSystemHealthPopupWidget *> m_systemHealthWidgets;
};

#endif // POPUP_COLLECTION_WIDGET_H
