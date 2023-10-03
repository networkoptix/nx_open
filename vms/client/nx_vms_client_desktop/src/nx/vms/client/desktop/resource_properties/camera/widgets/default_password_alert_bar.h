// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <ui/workbench/workbench_context_aware.h>

class QPushButton;

namespace nx::vms::client::desktop {

class DefaultPasswordAlertBar:
    public nx::vms::client::desktop::CommonMessageBar,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = nx::vms::client::desktop::CommonMessageBar;

public:
    explicit DefaultPasswordAlertBar(QWidget* parent = nullptr);
    virtual ~DefaultPasswordAlertBar() override;

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

} // namespace nx::vms::client::desktop
