#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnAligner;
namespace Ui { class WearableMotionWidget; }

class QnWearableMotionWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    explicit QnWearableMotionWidget(QWidget* parent = nullptr);
    virtual ~QnWearableMotionWidget() override;

    QnAligner* aligner() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResource(const QnVirtualCameraResourcePtr &camera);
    void submitToResource(const QnVirtualCameraResourcePtr &camera);

signals:
    void changed();

private:
    void updateSensitivityEnabled();

private:
    QScopedPointer<Ui::WearableMotionWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    QnAligner* m_aligner = nullptr;
    bool m_readOnly = false;
};
