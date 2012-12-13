#ifndef EMPTY_BUSINESS_ACTION_WIDGET_H
#define EMPTY_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

namespace Ui {
class QnEmptyBusinessActionWidget;
}

class QnEmptyBusinessActionWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnEmptyBusinessActionWidget(QWidget *parent = 0);
    ~QnEmptyBusinessActionWidget();
    
private:
    Ui::QnEmptyBusinessActionWidget *ui;
};

#endif // EMPTY_BUSINESS_ACTION_WIDGET_H
