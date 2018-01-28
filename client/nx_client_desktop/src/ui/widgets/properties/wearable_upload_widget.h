#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class WearableUploadWidget; }
namespace nx { namespace client { namespace desktop { struct WearableState; }}}

class QnWearableUploadWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit QnWearableUploadWidget(QWidget* parent = nullptr);
    virtual ~QnWearableUploadWidget() override;

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    QnVirtualCameraResourcePtr camera() const;

private:
    void updateFromState(const nx::client::desktop::WearableState& state);

    bool calculateEnabled(const nx::client::desktop::WearableState& state);
    QString calculateMessage(const nx::client::desktop::WearableState& state);

private:
    QScopedPointer<Ui::WearableUploadWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
};
