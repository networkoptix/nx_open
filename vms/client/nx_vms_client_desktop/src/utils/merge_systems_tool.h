#pragma once

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>

#include <api/model/getnonce_reply.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/merge_systems_common.h>

#include <nx/utils/url.h>
#include <nx/vms/api/data/module_information.h>

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
        const nx::vms::api::ModuleInformation& moduleInformation,
        const QnMediaServerResourcePtr& discoverer);
    void mergeFinished(
        utils::MergeSystemsStatus::Value mergeStatus,
        const nx::vms::api::ModuleInformation& moduleInformation,
        int requestHandle);

private:
    void at_pingSystem_finished(
        bool success,
        int handle,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const QString& errorString);

    void at_mergeSystem_finished(
        bool success,
        int handle,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const QString& errorString);

    void at_getNonceForMergeFinished(
        bool success,
        int handle,
        const QnGetNonceReply& nonce,
        const QString& errorString);

    void at_getNonceForPingFinished(
        bool success,
        int handle,
        const QnGetNonceReply& nonceReply,
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

        QString peerString() const;
    };

    QHash<int, TwoStepRequestCtx> m_twoStepRequests; //< getnonce/ping, getnonce/merge
    QPair<utils::MergeSystemsStatus::Value, nx::vms::api::ModuleInformation> m_foundModule;
};
