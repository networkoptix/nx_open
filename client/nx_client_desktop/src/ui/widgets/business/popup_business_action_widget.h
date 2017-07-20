#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class PopupBusinessActionWidget;
} // namespace Ui

class QnPopupBusinessActionWidget:
    public QnSubjectTargetActionWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit QnPopupBusinessActionWidget(QWidget* parent = nullptr);
    virtual ~QnPopupBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void at_settingsButton_clicked();

private:
    void parametersChanged();
    bool isRoleValid(const QnUuid& roleId) const;
    void updateValidationPolicy();

private:
    QScopedPointer<Ui::PopupBusinessActionWidget> ui;
};
