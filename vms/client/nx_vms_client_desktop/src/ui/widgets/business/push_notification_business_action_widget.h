#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui { class PushNotificationBusinessActionWidget; }

namespace nx::vms::client::desktop {

class PushNotificationBusinessActionWidget: public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit PushNotificationBusinessActionWidget(QWidget* parent = nullptr);
    virtual ~PushNotificationBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    void parametersChanged();

private:
    QScopedPointer<Ui::PushNotificationBusinessActionWidget> ui;
    QString m_lastCustomText;
};

} // namespace nx::vms::client::desktop