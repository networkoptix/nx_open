#pragma once

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <network/module_information.h>

class QnMergeSystemsTool : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    enum ErrorCode {
        InternalError = -1,
        NoError,
        VersionError,
        AuthentificationError,
        BackupError,
        NotFoundError,
        StarterLicenseError,
        SafeModeError,
        ForbiddenError
    };

    explicit QnMergeSystemsTool(QObject *parent = 0);

    void pingSystem(const QUrl &url, const QString &password);
    int mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &password, bool ownSettings);
    int configureIncompatibleServer(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &password);

signals:
    void systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode);
    void mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int requestHandle);

private slots:
    void at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);
    void at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);

private:
    QHash<int, QnMediaServerResourcePtr> m_serverByRequestHandle;
    QString m_adminPassword;
    QPair<ErrorCode, QnModuleInformation> m_foundModule;
};
