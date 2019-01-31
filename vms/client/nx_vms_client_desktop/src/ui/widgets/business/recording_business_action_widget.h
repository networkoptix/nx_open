#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
class RecordingBusinessActionWidget;
} // namespace Ui

class QnRecordingBusinessActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnRecordingBusinessActionWidget(QWidget* parent = nullptr);
    virtual ~QnRecordingBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::RecordingBusinessActionWidget> ui;
};
