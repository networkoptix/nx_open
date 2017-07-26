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

namespace {

static const std::chrono::hours kConnectionTimeout(10);
static const KeepAliveOptions kKeepAliveOptions(
    std::chrono::seconds(10), std::chrono::seconds(10), 3);

template<typename P>
bool allModulesMode(const P& p) { return p.value(lit("allModules")) == lit("true"); }

template<typename P>
bool showAddressesMode(const P& p) { return p.value(lit("showAddresses"), lit("true")) != lit("false"); }

template<typename P>
bool checkOwnerPermissionsMode(const P& p) { return p.value(lit("checkOwnerPermissions"), lit("false")) != lit("false"); }

template<typename P>
bool keepConnectionOpenMode(const P& p) { return p.contains(lit("keepConnectionOpen")); }

template<typename P>
bool updateStreamMode(const P& p) { return p.contains(lit("updateStream")); }

static void clearSockets(std::set<QSharedPointer<AbstractStreamSocket>>* sockets)
{
    for (const auto& s: *sockets)
        s->cancelIOSync(nx::network::aio::etNone);

    sockets->clear();
}

} // namespace

QnModuleInformationRestHandler::~QnModuleInformationRestHandler()
{
    nx::utils::promise<void> stopPromise;
    m_pollable.pleaseStop(
        [this, &stopPromise]()
        {
            NX_DEBUG(this, lm("Close all %1 kept and %2 updated connections on destruction")
                .args(m_socketsToKeepOpen.size(), m_socketsToUpdate.size()));

            clearSockets(&m_socketsToKeepOpen);
            clearSockets(&m_socketsToUpdate);
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
    if (checkOwnerPermissionsMode(params))
    {
        if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
            return QnPermissionsHelper::notOwnerError(result);
    }

    if (allModulesMode(params))
    {
        const auto allServers = owner->resourcePool()->getAllServers(Qn::AnyStatus);
        if (showAddressesMode(params))
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
    else if (showAddressesMode(params))
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

    // TODO: Probably owner is supposed to be passed as mutable.
    if (updateStreamMode(params))
        const_cast<QnRestConnectionProcessor*>(owner)->requestEmptyContentLength();

    return CODE_OK;
}

void QnModuleInformationRestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QnRestConnectionProcessor* owner)
{
    if (!keepConnectionOpenMode(params) && !updateStreamMode(params))
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

    if (!m_commonModule)
        m_commonModule = owner->commonModule();

    connect(m_commonModule, &QnCommonModule::moduleInformationChanged,
        this, &QnModuleInformationRestHandler::changeModuleInformation, Qt::UniqueConnection);

    m_pollable.post(
        [this, socket = std::move(socket), updateStreamMode = updateStreamMode(params)]()
        {
            if (updateStreamMode)
            {
                m_socketsToUpdate.insert(socket);
                NX_VERBOSE(this, lm("Connection %1 asks for update stream, %2 total")
                    .args(socket, m_socketsToKeepOpen.size()));

                sendModuleImformation(socket);
            }
            else
            {
                m_socketsToKeepOpen.insert(socket);
                NX_VERBOSE(this, lm("Connection %1 asks to keep connection open, %2 total")
                    .args(socket, m_socketsToKeepOpen.size()));

                auto buffer = std::make_shared<QByteArray>();
                buffer->reserve(100);
                socket->readSomeAsync(buffer.get(),
                    [this, socket, buffer](SystemError::ErrorCode code, size_t size)
                    {
                        m_socketsToKeepOpen.erase(socket);
                        NX_VERBOSE(this, lm("Unexpected event on connection from %1 (size=%2): %3")
                            .args(socket->getForeignAddress(), size, SystemError::toString(code)));
                    });
            }

        });

}

void QnModuleInformationRestHandler::changeModuleInformation()
{
    m_pollable.cancelPostedCalls(
        [this]()
        {
            NX_DEBUG(this, lm("Close %1 connections on moduleInformation change")
                .arg(m_socketsToKeepOpen.size()));
            clearSockets(&m_socketsToKeepOpen);

            updateModuleImformation();
            NX_DEBUG(this, lm("Update %1 connections on moduleInformation change")
                .arg(m_socketsToUpdate.size()));

            for (const auto& socket: m_socketsToUpdate)
                sendModuleImformation(socket);
        });
}


void QnModuleInformationRestHandler::updateModuleImformation()
{
    QnJsonRestResult result;
    result.setReply(m_commonModule->moduleInformation());
    m_moduleInformatiom = QJson::serialized(result);
}

void QnModuleInformationRestHandler::sendModuleImformation(
    const QSharedPointer<AbstractStreamSocket>& socket)
{
    if (m_moduleInformatiom.isEmpty())
        updateModuleImformation();

    socket->cancelIOSync(nx::network::aio::etNone);
    socket->sendAsync(m_moduleInformatiom,
        [this, socket](SystemError::ErrorCode code, size_t)
        {
            NX_DEBUG(this, lm("Update connection %1: %2")
                .args(socket, SystemError::toString(code)));

            if (code == SystemError::noError)
                return sendKeepAliveByTimer(socket);

            socket->cancelIOSync(nx::network::aio::etNone);
            m_socketsToUpdate.erase(socket);
        });
}


void QnModuleInformationRestHandler::sendKeepAliveByTimer(
    const QSharedPointer<AbstractStreamSocket>& socket)
{
    socket->registerTimer(kKeepAliveOptions.inactivityPeriodBeforeFirstProbe,
        [this, socket]()
        {
            static const nx::Buffer kKeepAlive("{}");
            socket->sendAsync(kKeepAlive,
                [this, socket](SystemError::ErrorCode code, size_t)
                {
                    if (code == SystemError::noError)
                        return sendKeepAliveByTimer(socket);

                    NX_DEBUG(this, lm("Unable to send keep alive toconnection %1: %2")
                        .args(socket, SystemError::toString(code)));

                    socket->cancelIOSync(nx::network::aio::etNone);
                    m_socketsToUpdate.erase(socket);
                });
        });
}

