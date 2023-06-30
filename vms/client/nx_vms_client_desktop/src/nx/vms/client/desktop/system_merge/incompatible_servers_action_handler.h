// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::api { struct ModuleInformation; }

class QnProgressDialog;
enum class MergeSystemsStatus;

namespace nx::vms::client::desktop {

class ConnectToCurrentSystemTool;
class MergeSystemsDialog;

// TODO: #ynikitenkov Rename class. Change "incompatible" to something more sensible (like 'fake')
class IncompatibleServersActionHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using ConnectToCurrentSystemTool = nx::vms::client::desktop::ConnectToCurrentSystemTool;

public:
    IncompatibleServersActionHandler(QObject* parent = nullptr);
    ~IncompatibleServersActionHandler();

private:
    void connectToCurrentSystem(const QnUuid& otherServerId);

    QString requestPassword() const;

    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();
    void at_connectTool_canceled();
    void at_connectTool_finished(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

private:
    QPointer<ConnectToCurrentSystemTool> m_connectTool;
    QPointer<MergeSystemsDialog> m_mergeDialog;
};

} // namespace nx::vms::client::desktop
