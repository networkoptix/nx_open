// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialog;
class UserSettingsDialog;
class GroupSettingsDialog;

} // namespace nx::vms::client::desktop

class QnServerSettingsDialog;
class QnUserSettingsDialog;

class QnWorkbenchResourcesSettingsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnWorkbenchResourcesSettingsHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchResourcesSettingsHandler();

private:
    void at_cameraSettingsAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_newUserAction_triggered();
    void at_userSettingsAction_triggered();
    void at_userRolesAction_triggered();
    void at_layoutSettingsAction_triggered();
    void at_currentLayoutSettingsAction_triggered();
    void at_copyRecordingScheduleAction_triggered();

private:
    void openLayoutSettingsDialog(const nx::vms::client::desktop::LayoutResourcePtr& layout);

private:
    QPointer<nx::vms::client::desktop::CameraSettingsDialog> m_cameraSettingsDialog;
    QPointer<QnServerSettingsDialog> m_serverSettingsDialog;
    QPointer<QnUserSettingsDialog> m_userSettingsDialog;
    QPointer<nx::vms::client::desktop::UserSettingsDialog> m_userSettingsDialog2;
    QPointer<nx::vms::client::desktop::GroupSettingsDialog> m_groupSettingsDialog2;
};
