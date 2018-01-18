#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class WearableUploadWidget; }

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
    QScopedPointer<Ui::WearableUploadWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
};
