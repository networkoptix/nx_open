#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

class QnConnectToCurrentSystemTool;
class QnProgressDialog;
class QnMergeSystemsDialog;

// TODO: #ynikitenkov Rename class. Change "incompatible" to something more sensible (like 'fake')
class QnWorkbenchIncompatibleServersActionHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWorkbenchIncompatibleServersActionHandler(QObject* parent = nullptr);
    ~QnWorkbenchIncompatibleServersActionHandler();

private:
    void connectToCurrentSystem(const QnFakeMediaServerResourcePtr& server);
    bool validateStartLicenses(const QnFakeMediaServerResourcePtr& server, const QString& adminPassword);
    bool serverHasStartLicenses(
        const QnMediaServerResourcePtr& server,
        const QString& adminPassword);

    QString requestPassword() const;

    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();
    void at_connectTool_finished(int errorCode);

private:
    QPointer<QnConnectToCurrentSystemTool> m_connectTool;
    QPointer<QnMergeSystemsDialog> m_mergeDialog;
};
