#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class ExitFullscreenActionWidget; }

class QnExitFullscreenActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnExitFullscreenActionWidget(QWidget* parent = nullptr);
    virtual ~QnExitFullscreenActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    void updateLayoutButton();
    void openLayoutSelectionDialog();

private:
    QScopedPointer<Ui::ExitFullscreenActionWidget> ui;
};
