#pragma once

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class AnalyticsSdkEventWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class AnalyticsSdkEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit AnalyticsSdkEventWidget(QWidget* parent = nullptr);
    ~AnalyticsSdkEventWidget();

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::AnalyticsSdkEventWidget> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx