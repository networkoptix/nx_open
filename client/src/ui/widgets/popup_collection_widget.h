#ifndef POPUP_COLLECTION_WIDGET_H
#define POPUP_COLLECTION_WIDGET_H

#include <QWidget>
#include <events/abstract_business_action.h>
#include <events/abstract_business_event.h>

namespace Ui {
    class QnPopupCollectionWidget;
}

class QnPopupCollectionWidget : public QWidget
{
    Q_OBJECT
    typedef QWidget base_type;
    
public:
    explicit QnPopupCollectionWidget(QWidget *parent);
    ~QnPopupCollectionWidget();

    void addExample();
    void addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
    void at_widget_closed(BusinessEventType::Value actionType, bool ignore);

private:
    void updatePosition();

private:
    Ui::QnPopupCollectionWidget *ui;

    QMap<BusinessEventType::Value, QWidget*> m_widgetsByType;
    QMap<BusinessEventType::Value, bool> m_ignore;
    bool m_adding;
};

#endif // POPUP_COLLECTION_WIDGET_H
