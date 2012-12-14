#ifndef EMPTY_BUSINESS_EVENT_WIDGET_H
#define EMPTY_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_event_widget.h>

namespace Ui {
class QnEmptyBusinessEventWidget;
}

class QnEmptyBusinessEventWidget : public QnAbstractBusinessEventWidget
{
    Q_OBJECT
    
public:
    explicit QnEmptyBusinessEventWidget(QWidget *parent = 0);
    ~QnEmptyBusinessEventWidget();
    
private:
    Ui::QnEmptyBusinessEventWidget *ui;
};

#endif // EMPTY_BUSINESS_EVENT_WIDGET_H
