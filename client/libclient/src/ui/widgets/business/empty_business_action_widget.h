#ifndef EMPTY_BUSINESS_ACTION_WIDGET_H
#define EMPTY_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class EmptyBusinessActionWidget;
}

class QnEmptyBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnEmptyBusinessActionWidget(QWidget *parent = 0);
    ~QnEmptyBusinessActionWidget();
private:
    QScopedPointer<Ui::EmptyBusinessActionWidget> ui;
};

#endif // EMPTY_BUSINESS_ACTION_WIDGET_H
