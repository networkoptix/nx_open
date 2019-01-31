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
            NX_DEBUG(this, "Closing all %1 kept and %2 updated connections on destruction",
                m_connectionsToKeepOpened.size(), m_connectionsToUpdate.size());

            m_pollable.pleaseStopSync();
            clear(&m_connectionsToKeepOpened);
            clear(&m_connectionsToUpdate);
            stopPromise.set_value();
        });

    stopPromise.get_future().wait();
}

void QnModuleInformationRestHandler::clear(Connections* connections)
{
    for (const auto& c: *connections)
        c.socket->cancelIOSync(nx::network::aio::etNone);

    connections->clear();
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
    const RestRequest& request, const QByteArray& /*response*/)
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
        NX_DEBUG(this, lm("Failed to configure a connection from %1: %2")
            .args(socket->getForeignAddress(), SystemError::toString(error)));
        return;
    }

    m_pollable.post(
        [this, socket = std::move(socket), updateStreamTime = updateStreamTime(request.params)
            ]() mutable
        {
            if (updateStreamTime)
            {
                const auto connection = m_connectionsToUpdate.insert(
                    m_connectionsToUpdate.end(), {std::move(socket), *updateStreamTime});

                NX_VERBOSE(this,
                    "A connection from %1 asks for updating a stream (interval %2), %3 total",
                    connection->socket->getForeignAddress(), *updateStreamTime,
                    m_connectionsToKeepOpened.size());

                sendKeepAliveByTimer(connection);
            }
            else
            {
                const auto connection = m_connectionsToKeepOpened.insert(
                    m_connectionsToKeepOpened.end(), {std::move(socket)});

                NX_VERBOSE(this, "A connection from %1 asks to keep connection opened, %2 total",
                    connection->socket->getForeignAddress(), m_connectionsToKeepOpened.size());

                auto buffer = std::make_shared<QByteArray>();
                buffer->reserve(100);
                connection->socket->readSomeAsync(buffer.get(),
                    [this, buffer, connection](SystemError::ErrorCode code, size_t size)
                    {
                        NX_VERBOSE(this,
                            "Unexpected event on the connection from %1 (size=%2): %3",
                            connection->socket->getForeignAddress(), size,
                            SystemError::toString(code));

                        connection->socket->cancelIOSync(nx::network::aio::etNone);
                        m_connectionsToKeepOpened.erase(connection);
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
            NX_DEBUG(this, "Close %1 connections on moduleInformation change",
                m_connectionsToKeepOpened.size());
            clear(&m_connectionsToKeepOpened);

            updateModuleImformation();
            NX_DEBUG(this, lm("Update %1 connections on moduleInformation change")
                .arg(m_connectionsToUpdate.size()));

            for (auto it = m_connectionsToUpdate.begin(); it != m_connectionsToUpdate.end(); ++it)
                sendModuleImformation(it);
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


void QnModuleInformationRestHandler::send(Connections::iterator connection, nx::Buffer buffer)
{
    connection->dataToSend.append(std::move(buffer));
    if (connection->isSendInProgress)
        return;

    connection->isSendInProgress = true;
    connection->socket->cancelIOSync(nx::network::aio::etNone); //< Stop timer.
    connection->socket->sendAsync(
        connection->dataToSend,
        [this, connection](SystemError::ErrorCode code, size_t size)
        {
            if (code != SystemError::noError)
            {
                NX_VERBOSE(this, "Connection from %1 is lost",
                    connection->socket->getForeignAddress());
                connection->socket->cancelIOSync(nx::network::aio::etNone);
                m_connectionsToUpdate.erase(connection);
                return;
            }

            connection->dataToSend.remove(0, (int) size);
            connection->isSendInProgress = false;
            if (connection->dataToSend.isEmpty())
                sendKeepAliveByTimer(connection, /*firstUpdate*/ false);
            else
                send(connection);
        });
}

void QnModuleInformationRestHandler::sendModuleImformation(Connections::iterator connection)
{
    if (m_moduleInformatiom.isEmpty())
        updateModuleImformation();

    NX_VERBOSE(this, "Sending module information to %1", connection->socket->getForeignAddress());
    send(connection, m_moduleInformatiom);
}

void QnModuleInformationRestHandler::sendKeepAliveByTimer(
    Connections::iterator connection, bool firstUpdate)
{
    auto interval = connection->updateInterval;
    using Duration = decltype(interval);
    if (firstUpdate)
        interval = Duration(nx::utils::random::number<Duration::rep>(1, interval.count()));

    connection->socket->registerTimer(
        interval,
        [this, connection, interval]()
        {
            NX_VERBOSE(this, "Send keep alive to %1", connection->socket->getForeignAddress());
            send(connection, "{}");
        });
}

