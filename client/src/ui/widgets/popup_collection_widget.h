#ifndef POPUP_COLLECTION_WIDGET_H
#define POPUP_COLLECTION_WIDGET_H

#include <QWidget>
#include <events/abstract_business_action.h>

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
    void at_widget_closed(BusinessActionType::Value actionType);

private:
    Ui::QnPopupCollectionWidget *ui;

    QMap<BusinessActionType::Value, QWidget*> m_widgetsByType;
    bool m_adding;
};

#endif // POPUP_COLLECTION_WIDGET_H
