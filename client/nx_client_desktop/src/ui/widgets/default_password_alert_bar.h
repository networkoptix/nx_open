#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/common/widgets/message_bar.h>

class QPushButton;

class QnDefaultPasswordAlertBar:
    public nx::client::desktop::AlertBar,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = nx::client::desktop::AlertBar;

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
    QPushButton* const m_setPasswordButton = nullptr;
    QnVirtualCameraResourceSet m_cameras;
    bool m_useMultipleForm = false;
};
