#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class WearableProgressWidget; }
namespace nx { namespace client { namespace desktop { struct WearableState; }}}

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
    void updateFromState(const nx::client::desktop::WearableState& state);

    bool calculateActive(const nx::client::desktop::WearableState& state);
    bool calculateCancelable(const nx::client::desktop::WearableState& state);
    QString calculateMessage(const nx::client::desktop::WearableState& state);
    QString calculateQueueMessage(const nx::client::desktop::WearableState& state);
    QString calculateFileName(const nx::client::desktop::WearableState& state);

    void setActive(bool active);

private:
    QScopedPointer<Ui::WearableProgressWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_active = false;
};
