#pragma once

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui { class BuzzerBusinessActionWidget; }

namespace nx::vms::client::desktop {

class BuzzerBusinessActionWidget: public QnSubjectTargetActionWidget
{
    Q_OBJECT
    typedef QnSubjectTargetActionWidget base_type;

public:
    explicit BuzzerBusinessActionWidget(QWidget* parent = nullptr);
    virtual ~BuzzerBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected:
    virtual void at_model_dataChanged(Fields fields) override;
    void setParamsToModel();

private:
    QScopedPointer<Ui::BuzzerBusinessActionWidget> m_ui;
};

} // namespace nx::vms::client::desktop
