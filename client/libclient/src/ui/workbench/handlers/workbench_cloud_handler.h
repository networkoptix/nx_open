#pragma once

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnLoginToCloudDialog;
class QnCloudUrlHelper;

class QnWorkbenchCloudHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnWorkbenchCloudHandler(QObject *parent = nullptr);
    virtual ~QnWorkbenchCloudHandler() override;

private:
    void at_loginToCloudAction_triggered();
    void at_logoutFromCloudAction_triggered();
    void at_openCloudMainUrlAction_triggered();
    void at_openCloudManagementUrlAction_triggered();

private:
    Q_DISABLE_COPY(QnWorkbenchCloudHandler)
    QPointer<QnLoginToCloudDialog> m_loginToCloudDialog;
    QnCloudUrlHelper* m_cloudUrlHelper;
};
