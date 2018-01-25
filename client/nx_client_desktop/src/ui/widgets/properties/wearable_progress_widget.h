#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

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

private:
    QScopedPointer<Ui::WearableProgressWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
};
