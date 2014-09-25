#include "merge_systems_tool.h"

#include <QtCore/QTimer>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkReply>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"
#include "common/common_module.h"
#include "utils/network/module_finder.h"

namespace {

    QnMergeSystemsTool::ErrorCode errorStringToErrorCode(const QString &str) {
        if (str.isEmpty())
            return QnMergeSystemsTool::NoError;
        if (str == lit("FAIL"))
            return QnMergeSystemsTool::NotFoundError;
        else if (str == lit("INCOMPATIBLE"))
            return QnMergeSystemsTool::VersionError;
        else if (str == lit("UNAUTHORIZED"))
            return QnMergeSystemsTool::AuthentificationError;
        else if (str == lit("BACKUP_ERROR"))
            return QnMergeSystemsTool::BackupError;
        else
            return QnMergeSystemsTool::InternalError;
    }

} // anonymous namespace

QnMergeSystemsTool::QnMergeSystemsTool(QObject *parent) :
    QObject(parent)
{
}

void QnMergeSystemsTool::pingSystem(const QUrl &url, const QString &password) {
    m_serverByRequestHandle.clear();
    foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllServers()) {
        if (server->getStatus() != Qn::Online)
            continue;

        int handle = server->apiConnection()->pingSystemAsync(url, password, this, SLOT(at_pingSystem_finished(int,QnModuleInformation,int,QString)));
        m_serverByRequestHandle[handle] = server;
    }
}

void QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &password, bool ownSettings) {
    proxy->apiConnection()->mergeSystemAsync(url, password, ownSettings, this, SLOT(at_mergeSystem_finished(int,QnModuleInformation,int,QString)));
}

void QnMergeSystemsTool::at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString) {
    QnMediaServerResourcePtr server = m_serverByRequestHandle.take(handle);
    if (!server)
        return;

    ErrorCode errorCode = errorStringToErrorCode(errorString);

    if (!errorString.isEmpty() || status != 0) {
        if (errorCode == NotFoundError) {
            if (m_serverByRequestHandle.isEmpty())
                emit systemFound(QnModuleInformation(), QnMediaServerResourcePtr(), NotFoundError);
        }

        m_serverByRequestHandle.clear();

        emit systemFound(moduleInformation, QnMediaServerResourcePtr(), errorCode);
        return;
    }

    m_serverByRequestHandle.clear();
    emit systemFound(moduleInformation, server, NoError);
}

void QnMergeSystemsTool::at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString) {
    Q_UNUSED(handle)

    if (status != 0)
        emit mergeFinished(InternalError, moduleInformation);
    else
        emit mergeFinished(errorStringToErrorCode(errorString), moduleInformation);
}
