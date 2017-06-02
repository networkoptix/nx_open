#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class ExecHttpRequestActionWidget;
}

class QnExecHttpRequestActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnExecHttpRequestActionWidget(QWidget *parent = 0);
    ~QnExecHttpRequestActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
private slots:
    void paramsChanged();
private:
    QScopedPointer<Ui::ExecHttpRequestActionWidget> ui;
};
