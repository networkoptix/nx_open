#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

class QnConnectToCurrentSystemTool;
class QnProgressDialog;
class QnMergeSystemsDialog;

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
    void connectToCurrentSystem(const QnUuid& target, const QString& initialPassword = QString());
    bool validateStartLicenses(const QnUuid& target, const QString& adminPassword);
    bool serverHasStartLicenses(
        const QnMediaServerResourcePtr& server,
        const QString& adminPassword);

    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();
    void at_connectTool_finished(int errorCode);

private:
    QPointer<QnConnectToCurrentSystemTool> m_connectTool;
    QPointer<QnMergeSystemsDialog> m_mergeDialog;
};
