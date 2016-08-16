#pragma once

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <network/module_information.h>
#include <api/model/getnonce_reply.h>

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
        ForbiddenError,
        ConfigurationError,
        DependentSystemBoundToCloudError,
        UnconfiguredSystemError,
        notLocalOwner
    };

    explicit QnMergeSystemsTool(QObject *parent = 0);

    void pingSystem(const QUrl &url, const QAuthenticator& auth);
    int mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QAuthenticator& userAuth, bool ownSettings);
    int configureIncompatibleServer(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QAuthenticator& auth);

signals:
    void systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode);
    void mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int requestHandle);

private slots:
    void at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);
    void at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString);
    void at_getNonceForMergeFinished(int status, const QnGetNonceReply& nonce, int handle, const QString& errorString);
    void at_getNonceForPingFinished(int status, const QnGetNonceReply& nonceReply, int handle, const QString& errorString);
private:
    struct TwoStepRequestCtx
    {
        TwoStepRequestCtx():
            ownSettings(false),
            oneServer(false),
            ignoreIncompatible(false),
            nonceRequestHandle(-1),
            mainRequestHandle(-1)
        {
        }

        QnMediaServerResourcePtr proxy;
        QUrl url;
        QAuthenticator auth;
        bool ownSettings;
        bool oneServer;
        bool ignoreIncompatible;

        int nonceRequestHandle;
        int mainRequestHandle;
    };

    QHash<int, TwoStepRequestCtx> m_twoStepRequests; //< getnonce/ping, getnonce/merge
    QPair<ErrorCode, QnModuleInformation> m_foundModule;
};
