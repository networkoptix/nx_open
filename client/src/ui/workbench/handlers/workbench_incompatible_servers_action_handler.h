#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <utils/common/uuid.h>


class QnConnectToCurrentSystemTool;
class QnProgressDialog;
class QnMergeSystemsDialog;

class QnWorkbenchIncompatibleServersActionHandler : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    explicit QnWorkbenchIncompatibleServersActionHandler(QObject *parent = 0);
    ~QnWorkbenchIncompatibleServersActionHandler();

protected slots:
    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();

private:
    void connectToCurrentSystem(const QSet<QnUuid> &targets, const QString &initialPassword = QString());
    bool validateStartLicenses(const QSet<QnUuid> &targets, const QString &adminPassword);
    bool serverHasStartLicenses(const QnMediaServerResourcePtr &server, const QString &adminPassword);

private slots:
    void at_connectTool_finished(int errorCode);

private:
    QPointer<QnConnectToCurrentSystemTool> m_connectTool;
    QPointer<QnMergeSystemsDialog> m_mergeDialog;
};
