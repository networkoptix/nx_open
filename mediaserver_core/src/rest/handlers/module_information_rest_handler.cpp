#include "module_information_rest_handler.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/module_information.h>
#include <network/tcp_connection_priv.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

QnModuleInformationRestHandler::~QnModuleInformationRestHandler()
{
    closeAllSockets();

    QnMutexLocker lock(&m_mutex);
    while (!m_savedSockets.empty())
        m_condition.wait(&m_mutex);
}

int QnModuleInformationRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    bool allModules = params.value(lit("allModules")) == lit("true");
    bool useAddresses = params.value(lit("showAddresses"), lit("true")) != lit("false");
    bool checkOwnerPermissions = params.value(lit("checkOwnerPermissions"), lit("false")) != lit("false");

    if (checkOwnerPermissions)
    {
        if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
            return QnPermissionsHelper::notOwnerError(result);
    }

    if (allModules)
    {
        const auto allServers = owner->resourcePool()->getAllServers(Qn::AnyStatus);
        if (useAddresses)
        {
            QList<QnModuleInformationWithAddresses> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
                modules.append(std::move(server->getModuleInformationWithAddresses()));

            result.setReply(modules);
        }
        else
        {
            QList<QnModuleInformation> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
                modules.append(server->getModuleInformation());
            result.setReply(modules);
        }
    }
    else if (useAddresses)
    {
        const auto id = owner->commonModule()->moduleGUID();
        if (const auto s = owner->resourcePool()->getResourceById<QnMediaServerResource>(id))
            result.setReply(s->getModuleInformationWithAddresses());
        else
            result.setReply(owner->commonModule()->moduleInformation());
    }
    else
    {
        result.setReply(owner->commonModule()->moduleInformation());
    }

    return CODE_OK;
}

void QnModuleInformationRestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QnRestConnectionProcessor* owner)
{
    if (!params.contains(lit("keepConnectionOpen")))
        return;

    // TODO: Probably owner is supposed to be passed as mutable.
    const auto socket = const_cast<QnRestConnectionProcessor*>(owner)->takeSocket();
    if (!socket->setNonBlockingMode(true) || !socket->setRecvTimeout(0))
    {
        NX_WARNING(this) "Failed to configure" << socket.data();
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);
        m_savedSockets.insert(socket);
        NX_DEBUG(this, lm("%1 asks to keep connection open, %2 total")
            .args(socket.data(), m_savedSockets.size()));
    }

    connect(owner->commonModule(), &QnCommonModule::moduleInformationChanged,
        this, &QnModuleInformationRestHandler::closeAllSockets, Qt::UniqueConnection);

    auto buffer = std::make_shared<QByteArray>();
    buffer->reserve(100);
    socket->readSomeAsync(buffer.get(),
        [this, socket, buffer](SystemError::ErrorCode code, size_t size)
        {
            NX_DEBUG(this, lm("Unexpected event on %1 (size=%2): %3")
                .args(socket.data(), size, SystemError::toString(code)));
            removeSocket(socket);
        });
}

void QnModuleInformationRestHandler::closeAllSockets()
{
    std::set<QSharedPointer<AbstractStreamSocket>> savedSockets;
    {
        QnMutexLocker lock(&m_mutex);
        savedSockets = m_savedSockets;
    }

    NX_DEBUG(this, lm("Close all %1 sockets").arg(m_savedSockets.size()));
    for (const auto& socket: savedSockets)
        socket->pleaseStop([this, socket]() { removeSocket(socket); });
}

void QnModuleInformationRestHandler::removeSocket(const QSharedPointer<AbstractStreamSocket>& socket)
{
    QnMutexLocker lock(&m_mutex);
    m_savedSockets.erase(socket);
    m_condition.wakeAll();
}
