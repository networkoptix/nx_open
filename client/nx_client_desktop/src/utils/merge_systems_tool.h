#pragma once

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <network/module_information.h>
#include <api/model/getnonce_reply.h>
#include <utils/merge_systems_common.h>
#include <nx/utils/url.h>

class QnMergeSystemsTool: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    QnMergeSystemsTool(QObject* parent = nullptr);

    void pingSystem(const nx::utils::Url& url, const QAuthenticator& auth);
    int mergeSystem(
        const QnMediaServerResourcePtr& proxy,
        const nx::utils::Url& url,
        const QAuthenticator& userAuth,
        bool ownSettings);
    int configureIncompatibleServer(const QnMediaServerResourcePtr& proxy,
        const nx::utils::Url &url,
        const QAuthenticator& auth);

  signals:
    void systemFound(
        utils::MergeSystemsStatus::Value mergeStatus,
        const QnModuleInformation& moduleInformation,
        const QnMediaServerResourcePtr& discoverer);
    void mergeFinished(
        utils::MergeSystemsStatus::Value mergeStatus,
        const QnModuleInformation& moduleInformation,
        int requestHandle);

private slots:
    void at_pingSystem_finished(
        int status,
        const QnModuleInformation& moduleInformation,
        int handle,
        const QString& errorString);
    void at_mergeSystem_finished(
        int status,
        const QnModuleInformation& moduleInformation,
        int handle,
        const QString& errorString);
    void at_getNonceForMergeFinished(
        int status,
        const QnGetNonceReply& nonce,
        int handle,
        const QString& errorString);
    void at_getNonceForPingFinished(
        int status,
        const QnGetNonceReply& nonceReply,
        int handle,
        const QString& errorString);

private:
    struct TwoStepRequestCtx
    {
        QnMediaServerResourcePtr proxy;
        nx::utils::Url url;
        QAuthenticator auth;
        bool ownSettings = false;
        bool oneServer = false;
        bool ignoreIncompatible = false;
        int nonceRequestHandle = -1;
        int mainRequestHandle = -1;
    };

    QHash<int, TwoStepRequestCtx> m_twoStepRequests; //< getnonce/ping, getnonce/merge
    QPair<utils::MergeSystemsStatus::Value, QnModuleInformation> m_foundModule;
};
