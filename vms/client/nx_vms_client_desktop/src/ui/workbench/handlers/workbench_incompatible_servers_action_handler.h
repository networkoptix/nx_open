#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <core/resource/client_resource_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop { class ConnectToCurrentSystemTool; }
class QnProgressDialog;
class QnMergeSystemsDialog;

// TODO: #ynikitenkov Rename class. Change "incompatible" to something more sensible (like 'fake')
class QnWorkbenchIncompatibleServersActionHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
    using ConnectToCurrentSystemTool = nx::vms::client::desktop::ConnectToCurrentSystemTool;

public:
    QnWorkbenchIncompatibleServersActionHandler(QObject* parent = nullptr);
    ~QnWorkbenchIncompatibleServersActionHandler();

private:
    void connectToCurrentSystem(const QnFakeMediaServerResourcePtr& server);

    /**
     * Some license types can exist only once in the system (Starter, Nvr). Check if the local
     * system has no conflicts with remote system over those license types.
     * @return true if there is no conflict or if user is ok to deactivate conflicting licenses.
     */
    bool validateUniqueLicenses(
        const QnFakeMediaServerResourcePtr& server,
        const QString& adminPassword);

    QString requestPassword() const;

    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();
    void at_connectTool_finished(int errorCode);

private:
    QPointer<ConnectToCurrentSystemTool> m_connectTool;
    QPointer<QnMergeSystemsDialog> m_mergeDialog;
};
