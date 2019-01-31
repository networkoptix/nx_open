#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/vms/client/desktop/utils/wearable_fwd.h>

namespace Ui { class WearableProgressWidget; }

class QnWearableProgressWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit QnWearableProgressWidget(QWidget* parent = nullptr);
    virtual ~QnWearableProgressWidget() override;

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    QnVirtualCameraResourcePtr camera() const;

    bool isActive();

signals:
    void activeChanged();

private:
    void updateFromState(const nx::vms::client::desktop::WearableState& state);

    bool calculateActive(const nx::vms::client::desktop::WearableState& state);
    QString calculateMessage(const nx::vms::client::desktop::WearableState& state);
    QString calculateQueueMessage(const nx::vms::client::desktop::WearableState& state);
    QString calculateFileName(const nx::vms::client::desktop::WearableState& state);

    void setActive(bool active);

private:
    QScopedPointer<Ui::WearableProgressWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_active = false;
};
