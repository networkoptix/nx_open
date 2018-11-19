#include "module_information_rest_handler.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/api/data/module_information.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

namespace {

static const std::chrono::hours kConnectionTimeout(10);
static const nx::network::KeepAliveOptions kKeepAliveOptions(
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
std::optional<std::chrono::milliseconds> updateStreamTime(const P& p)
{
    if (!p.contains("updateStream"))
        return std::nullopt;

    std::chrono::seconds requestedTime(p.value("updateStream").toInt());
    if (requestedTime.count() == 0)
        requestedTime = kKeepAliveOptions.inactivityPeriodBeforeFirstProbe / 2;

    return std::chrono::milliseconds(requestedTime);
}

} // namespace

QnModuleInformationRestHandler::QnModuleInformationRestHandler(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    Qn::directConnect(commonModule, &QnCommonModule::moduleInformationChanged, this,
        &QnModuleInformationRestHandler::changeModuleInformation);
}

QnModuleInformationRestHandler::~QnModuleInformationRestHandler()
{
    directDisconnectAll();

    {
        QnMutex m_mutex;
        m_aboutToStop = true;
    }

    nx::network::aio::BasicPollable stopPollable(m_pollable.getAioThread());

    nx::utils::promise<void> stopPromise;
    stopPollable.post(
        [this, &stopPromise]()
        {
            m_pollable.pleaseStopSync();
            NX_DEBUG(this, lm("Close all %1 kept and %2 updated connections on destruction")
                .args(m_socketsToKeepOpen.size(), m_socketsToUpdate.size()));

            clearSockets(&m_socketsToKeepOpen);
            clearSockets(&m_socketsToUpdate);
            stopPromise.set_value();
        });

    stopPromise.get_future().wait();
}

void QnModuleInformationRestHandler::clearSockets(SocketList* sockets)
{
    for (const auto& s: *sockets)
        s.first->cancelIOSync(nx::network::aio::etNone);

    sockets->clear();
}

JsonRestResponse QnModuleInformationRestHandler::executeGet(const JsonRestRequest& request)
{
    if (checkOwnerPermissionsMode(request.params)
        && !QnPermissionsHelper::hasOwnerPermissions(
            request.owner->resourcePool(), request.owner->accessRights()))
    {
        JsonRestResponse response;
        response.statusCode = (nx::network::http::StatusCode::Value)
            QnPermissionsHelper::notOwnerError(response.json);
        return response;
    }

    JsonRestResponse response(nx::network::http::StatusCode::ok, {},
        (bool) updateStreamTime(request.params));
    if (allModulesMode(request.params))
    {
        const auto allServers = request.owner->resourcePool()->getAllServers(Qn::AnyStatus);
        if (showAddressesMode(request.params))
        {
            QList<nx::vms::api::ModuleInformationWithAddresses> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
                modules.append(std::move(server->getModuleInformationWithAddresses()));

            response.json.setReply(modules);
        }
        else
        {
            QList<nx::vms::api::ModuleInformation> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
                modules.append(server->getModuleInformation());
            response.json.setReply(modules);
        }
    }
    else if (showAddressesMode(request.params))
    {
        const auto id = request.owner->commonModule()->moduleGUID();
        if (const auto s = request.owner->resourcePool()->getResourceById<QnMediaServerResource>(id))
            response.json.setReply(s->getModuleInformationWithAddresses());
        else
            response.json.setReply(request.owner->commonModule()->moduleInformation());
    }
    else
    {
        response.json.setReply(request.owner->commonModule()->moduleInformation());
    }

    response.statusCode = nx::network::http::StatusCode::ok;
    if (updateStreamTime(request.params))
        response.isUndefinedContentLength = true;

    return response;
}

void QnModuleInformationRestHandler::afterExecute(
    const RestRequest& request, const QByteArray& response)
{
    if (!keepConnectionOpenMode(request.params) && !updateStreamTime(request.params))
        return;

    // TODO: Probably owner is supposed to be passed as mutable.
    std::unique_ptr<nx::network::AbstractStreamSocket> socket =
        const_cast<QnRestConnectionProcessor*>(request.owner)->takeSocket();

    socket->bindToAioThread(m_pollable.getAioThread());
    if (!socket->setNonBlockingMode(true)
        || !socket->setRecvTimeout(kConnectionTimeout)
        || !socket->setKeepAlive(kKeepAliveOptions))
    {
        const auto error = SystemError::getLastOSErrorCode();
        NX_DEBUG(this, lm("Failed to configure connection from %1: %2")
            .args(socket->getForeignAddress(), SystemError::toString(error)));
        return;
    }

    m_pollable.post(
        [this, socket = std::move(socket), updateStreamTime = updateStreamTime(request.params)
            ]() mutable
        {
            auto socketPtr = socket.get();

            if (updateStreamTime)
            {
                NX_VERBOSE(this, lm("Connection %1 asks for update stream (interval %2), %3 total")
                    .args(socket, *updateStreamTime, m_socketsToKeepOpen.size()));

                m_socketsToUpdate.emplace(socketPtr, std::move(socket));
                sendKeepAliveByTimer(socketPtr, *updateStreamTime);
            }
            else
            {
                m_socketsToKeepOpen.emplace(socketPtr, std::move(socket));
                NX_VERBOSE(this, lm("Connection %1 asks to keep connection open, %2 total")
                    .args(socketPtr, m_socketsToKeepOpen.size()));

                auto buffer = std::make_shared<QByteArray>();
                buffer->reserve(100);
                socketPtr->readSomeAsync(buffer.get(),
                    [this, socketPtr, buffer](SystemError::ErrorCode code, size_t size)
                    {
                        socketPtr->cancelIOSync(nx::network::aio::etNone);
                        NX_VERBOSE(this, lm("Unexpected event on connection from %1 (size=%2): %3")
                            .args(socketPtr->getForeignAddress(), size, SystemError::toString(code)));
                        m_socketsToKeepOpen.erase(socketPtr);
                });
            }
        });
}

void QnModuleInformationRestHandler::changeModuleInformation()
{
    QnMutexLocker lock(&m_mutex);
    if (m_aboutToStop)
        return;

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
                sendModuleImformation(socket.first);
        });
}

int QnModuleInformationRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& /*result*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    NX_ASSERT(false, "Is not supposed to be called");
    return (int) nx::network::http::StatusCode::notImplemented;
}

void QnModuleInformationRestHandler::updateModuleImformation()
{
    QnJsonRestResult result;
    result.setReply(commonModule()->moduleInformation());
    m_moduleInformatiom = QJson::serialized(result);
}

void QnModuleInformationRestHandler::sendModuleImformation(
    nx::network::AbstractStreamSocket* socket)
{
    if (m_moduleInformatiom.isEmpty())
        updateModuleImformation();

    socket->sendAsync(m_moduleInformatiom,
        [this, socket](SystemError::ErrorCode code, size_t)
        {
            NX_DEBUG(this, lm("Update connection %1: %2")
                .args(socket, SystemError::toString(code)));

            if (code == SystemError::noError)
                return;

            socket->cancelIOSync(nx::network::aio::etNone);
            m_socketsToUpdate.erase(socket);
        });
}


void QnModuleInformationRestHandler::sendKeepAliveByTimer(
    nx::network::AbstractStreamSocket* socket, std::chrono::milliseconds interval, bool firstUpdate)
{
    using Duration = decltype(interval);
    if (firstUpdate)
        interval = Duration(nx::utils::random::number<Duration::rep>(1, interval.count()));

    socket->registerTimer(interval,
        [this, socket, interval]()
        {
            static const auto kEmptyObject = QJson::serialize(QJsonObject());
            socket->sendAsync(kEmptyObject,
                [this, socket, interval](SystemError::ErrorCode code, size_t)
                {
                    if (code == SystemError::noError)
                        return sendKeepAliveByTimer(socket, interval, /*firstUpdate*/ false);

                    NX_DEBUG(this, lm("Unable to send keep alive to connection %1: %2")
                        .args(socket, SystemError::toString(code)));

                    socket->cancelIOSync(nx::network::aio::etNone);
                    m_socketsToUpdate.erase(socket);
                });
        });
}

