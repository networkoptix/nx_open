#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui {
class SayTextBusinessActionWidget;
} // namespace Ui

class QnSayTextBusinessActionWidget: public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit QnSayTextBusinessActionWidget(QWidget* parent = nullptr);
    virtual ~QnSayTextBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();
    void enableTestButton();
    void at_testButton_clicked();
    void at_volumeSlider_valueChanged(int value);

private:
    QScopedPointer<Ui::SayTextBusinessActionWidget> ui;
};
