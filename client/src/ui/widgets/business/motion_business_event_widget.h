#ifndef MOTION_BUSINESS_EVENT_WIDGET_H
#define MOTION_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnMotionBusinessEventWidget;
}

class QnMotionBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnMotionBusinessEventWidget(QWidget *parent = 0);
    ~QnMotionBusinessEventWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override {}
    virtual QnBusinessParams parameters() override {return QnBusinessParams(); }

private:
    Ui::QnMotionBusinessEventWidget *ui;
};

#endif // MOTION_BUSINESS_EVENT_WIDGET_H
