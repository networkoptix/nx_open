#ifndef EMPTY_BUSINESS_EVENT_WIDGET_H
#define EMPTY_BUSINESS_EVENT_WIDGET_H

#include <QWidget>

namespace Ui {
class QnEmptyBusinessEventWidget;
}

class QnEmptyBusinessEventWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnEmptyBusinessEventWidget(QWidget *parent = 0);
    ~QnEmptyBusinessEventWidget();
    
private:
    Ui::QnEmptyBusinessEventWidget *ui;
};

#endif // EMPTY_BUSINESS_EVENT_WIDGET_H
