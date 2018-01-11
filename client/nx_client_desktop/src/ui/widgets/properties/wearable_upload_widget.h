#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class WearableUploadWidget; }

class QnWearableUploadWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QWidget base_type;

public:
    QnWearableUploadWidget(QWidget *parent = nullptr);
    virtual ~QnWearableUploadWidget() override;

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    QnVirtualCameraResourcePtr camera() const;

private:
    QScopedPointer<Ui::WearableUploadWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
};

