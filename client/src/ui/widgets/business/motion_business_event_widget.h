#ifndef MOTION_BUSINESS_EVENT_WIDGET_H
#define MOTION_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_event_widget.h>

namespace Ui {
class QnMotionBusinessEventWidget;
}

class QnMotionBusinessEventWidget : public QnAbstractBusinessEventWidget
{
    Q_OBJECT
    
public:
    explicit QnMotionBusinessEventWidget(QWidget *parent = 0);
    ~QnMotionBusinessEventWidget();
    
private:
    Ui::QnMotionBusinessEventWidget *ui;
};

#endif // MOTION_BUSINESS_EVENT_WIDGET_H
