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

static const std::chrono::hours kConnectionTimeout(10);
static const KeepAliveOptions kKeepAliveOptions(
    std::chrono::seconds(60), std::chrono::seconds(10), 3);

QnModuleInformationRestHandler::~QnModuleInformationRestHandler()
{
    nx::utils::promise<void> stopPromise;
    m_pollable.pleaseStop(
        [this, &stopPromise]()
        {
            NX_DEBUG(this, lm("Close all %1 connections on destruction")
                .arg(m_savedSockets.size()));

            m_savedSockets.clear();
            stopPromise.set_value();
        });

    stopPromise.get_future().wait();
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
    auto socket = const_cast<QnRestConnectionProcessor*>(owner)->takeSocket();
    socket->bindToAioThread(m_pollable.getAioThread());
    if (!socket->setNonBlockingMode(true)
        || !socket->setRecvTimeout(kConnectionTimeout)
        || !socket->setKeepAlive(kKeepAliveOptions))
    {
        const auto error = SystemError::getLastOSErrorCode();
        NX_WARNING(this, lm("Failed to configure connection from %1: %2")
            .args(socket->getForeignAddress(), SystemError::toString(error)));
        return;
    }

    connect(owner->commonModule(), &QnCommonModule::moduleInformationChanged,
        this, &QnModuleInformationRestHandler::changeModuleInformation, Qt::UniqueConnection);

    m_pollable.post(
        [this, socket = std::move(socket)]()
        {
            m_savedSockets.insert(socket);
            NX_VERBOSE(this, lm("Connection from %1 asks to keep connection open, %2 total")
                .args(socket->getForeignAddress(), m_savedSockets.size()));

            auto buffer = std::make_shared<QByteArray>();
            buffer->reserve(100);
            socket->readSomeAsync(buffer.get(),
                [this, socket, buffer](SystemError::ErrorCode code, size_t size)
                {
                    m_savedSockets.erase(socket);
                    NX_VERBOSE(this, lm("Unexpected event on connection from %1 (size=%2): %3")
                        .args(socket->getForeignAddress(), size, SystemError::toString(code)));
                });
        });
}

void QnModuleInformationRestHandler::changeModuleInformation()
{
    m_pollable.cancelPostedCalls(
        [this]()
        {
            NX_DEBUG(this, lm("Close all %1 connections on moduleInformation change")
                .arg(m_savedSockets.size()));

            m_savedSockets.clear();
        });
}
