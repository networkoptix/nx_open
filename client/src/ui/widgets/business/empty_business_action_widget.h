#ifndef EMPTY_BUSINESS_ACTION_WIDGET_H
#define EMPTY_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
class QnEmptyBusinessActionWidget;
}

class QnEmptyBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnEmptyBusinessActionWidget(QWidget *parent = 0);
    ~QnEmptyBusinessActionWidget();

    virtual void loadParameters(const QnBusinessParams &params) override {}
    virtual QnBusinessParams parameters() const override {return QnBusinessParams(); }
    virtual QString description() const override;
private:
    Ui::QnEmptyBusinessActionWidget *ui;
};

#endif // EMPTY_BUSINESS_ACTION_WIDGET_H
