#ifndef MOTION_BUSINESS_EVENT_WIDGET_H
#define MOTION_BUSINESS_EVENT_WIDGET_H

#include <QWidget>

namespace Ui {
class QnMotionBusinessEventWidget;
}

class QnMotionBusinessEventWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnMotionBusinessEventWidget(QWidget *parent = 0);
    ~QnMotionBusinessEventWidget();
    
private:
    Ui::QnMotionBusinessEventWidget *ui;
};

#endif // MOTION_BUSINESS_EVENT_WIDGET_H
