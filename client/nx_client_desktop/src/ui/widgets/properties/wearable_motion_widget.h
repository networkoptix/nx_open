#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnAligner;
namespace Ui { class WearableMotionWidget; }

class QnWearableMotionWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit QnWearableMotionWidget(QWidget* parent = nullptr);
    virtual ~QnWearableMotionWidget() override;

    QnAligner* aligner() const;

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    QnVirtualCameraResourcePtr camera() const;

private:
    void updateSensitivityEnabled();

private:
    QScopedPointer<Ui::WearableMotionWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    QnAligner* m_aligner = nullptr;
};
