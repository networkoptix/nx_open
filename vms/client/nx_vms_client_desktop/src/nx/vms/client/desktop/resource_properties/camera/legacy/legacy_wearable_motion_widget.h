#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class WearableMotionWidget; }
namespace nx::vms::client::desktop { class Aligner; }

class QnWearableMotionWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    explicit QnWearableMotionWidget(QWidget* parent = nullptr);
    virtual ~QnWearableMotionWidget() override;

    nx::vms::client::desktop::Aligner* aligner() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResource(const QnVirtualCameraResourcePtr &camera);
    void submitToResource(const QnVirtualCameraResourcePtr &camera);

signals:
    void changed();

private:
    void updateSensitivityEnabled();

    int calculateSensitivity(const QnVirtualCameraResourcePtr &camera) const;
    void submitSensitivity(const QnVirtualCameraResourcePtr &camera, int sensitivity) const;

private:
    QScopedPointer<Ui::WearableMotionWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    nx::vms::client::desktop::Aligner* m_aligner = nullptr;
    bool m_readOnly = false;
};
