#ifndef POPUP_COLLECTION_WIDGET_H
#define POPUP_COLLECTION_WIDGET_H

#include <QWidget>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupCollectionWidget;
}

class QnPopupCollectionWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QWidget base_type;
    
public:
    explicit QnPopupCollectionWidget(QWidget *parent, QnWorkbenchContext *context = NULL);
    ~QnPopupCollectionWidget();

    bool addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
    void updatePosition();

    void at_widget_closed(BusinessEventType::Value actionType, bool ignore);

private:
    Ui::QnPopupCollectionWidget *ui;

    QMap<BusinessEventType::Value, QWidget *> m_widgetsByType;
};

#endif // POPUP_COLLECTION_WIDGET_H
