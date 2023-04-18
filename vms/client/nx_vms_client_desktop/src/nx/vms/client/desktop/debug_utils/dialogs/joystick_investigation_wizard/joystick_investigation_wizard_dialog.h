// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/workbench/workbench_context_aware.h>

class QQuickWidget;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

namespace joystick { class JoystickManager; }

class JoystickInvestigationWizardDialog: public QmlDialogWrapper, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    JoystickInvestigationWizardDialog(QWidget* parent);
    virtual ~JoystickInvestigationWizardDialog() override;

public slots:
    void onResultReady(const QString& filePath, const QString& jsonData);

private:
    std::unique_ptr<joystick::JoystickManager> m_joystickManager;

public:
    static void registerAction();
};

} // namespace nx::vms::client::desktop
