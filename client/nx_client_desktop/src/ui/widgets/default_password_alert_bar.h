#pragma once

#include <ui/widgets/common/alert_bar.h>
#include <ui/workbench/workbench_context_aware.h>

#include <core/resource/resource_fwd.h>

class QPushButton;

class QnDefaultPasswordAlertBar: public QnAlertBar, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnWorkbenchContextAware;

public:
    explicit QnDefaultPasswordAlertBar(QWidget* parent = nullptr);
    virtual ~QnDefaultPasswordAlertBar() override;

    QnVirtualCameraResourceSet cameras() const;
    void setCameras(const QnVirtualCameraResourceSet& cameras);

    bool useMultipleForm() const;
    void setUseMultipleForm(bool value);

private:
    void updateState();

signals:
    void changeDefaultPasswordRequest();
    void targetCamerasChanged();

private:
    QPushButton* m_setPasswordButton = nullptr;
    QnVirtualCameraResourceSet m_cameras;
    bool m_useMultipleForm = false;
};
