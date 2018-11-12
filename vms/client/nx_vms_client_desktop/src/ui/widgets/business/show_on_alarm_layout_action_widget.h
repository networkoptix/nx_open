#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui {
class ShowOnAlarmLayoutActionWidget;
} // namespace Ui

class QnShowOnAlarmLayoutActionWidget: public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit QnShowOnAlarmLayoutActionWidget(QWidget* parent = nullptr);
    virtual ~QnShowOnAlarmLayoutActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    void paramsChanged();

private:
    QScopedPointer<Ui::ShowOnAlarmLayoutActionWidget> ui;
};
