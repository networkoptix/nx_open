#ifndef EMPTY_BUSINESS_EVENT_WIDGET_H
#define EMPTY_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
class QnEmptyBusinessEventWidget;
}

class QnEmptyBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnEmptyBusinessEventWidget(QWidget *parent = 0);
    ~QnEmptyBusinessEventWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override { Q_UNUSED(params) }
    virtual QnBusinessParams parameters() const override {return QnBusinessParams(); }
    virtual QString description() const override;
private:
    Ui::QnEmptyBusinessEventWidget *ui;
};

#endif // EMPTY_BUSINESS_EVENT_WIDGET_H
