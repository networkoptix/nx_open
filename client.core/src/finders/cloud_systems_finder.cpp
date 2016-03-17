
#include "cloud_systems_finder.h"

#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <utils/common/model_functions.h>
#include <nx/utils/raii_guard.h>
#include <nx/network/socket_global.h>
#include <common/common_module.h>
#include <nx/network/http/asynchttpclient.h>

namespace
{
    enum 
    {
        kCloudSystemsRefreshPeriod = 10 * 1000          // 10 seconds
    };
}

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_updateSystemsTimer(new QTimer(this))
    , m_mutex()
    , m_systems()
    , m_requestToSystem()
{
    const auto cloudWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    NX_ASSERT(cloudWatcher, Q_FUNC_INFO, "Cloud watcher is not ready");

    connect(cloudWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(cloudWatcher, &QnCloudStatusWatcher::cloudSystemsChanged
        , this, &QnCloudSystemsFinder::setCloudSystems);
    connect(cloudWatcher, &QnCloudStatusWatcher::error
        , this, &QnCloudSystemsFinder::onCloudError);

    connect(m_updateSystemsTimer, &QTimer::timeout
        , this, &QnCloudSystemsFinder::updateSystems);
    m_updateSystemsTimer->setInterval(kCloudSystemsRefreshPeriod);
    m_updateSystemsTimer->start();
}

QnCloudSystemsFinder::~QnCloudSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnCloudSystemsFinder::systems() const
{
    const QnMutexLocker lock(&m_mutex);
    return m_systems.values();
}

void QnCloudSystemsFinder::onCloudStatusChanged(QnCloudStatusWatcher::Status status)
{
    switch (status)
    {
    case QnCloudStatusWatcher::Online:
    {
        const auto cloudWatcher = qnCommon->instance<QnCloudStatusWatcher>();
//        cloudWatcher->updateSystems();
        break;
    }

    case QnCloudStatusWatcher::LoggedOut:
    case QnCloudStatusWatcher::Unauthorized:
    case QnCloudStatusWatcher::Offline:
//        setCloudSystems(QnCloudSystemList());
        break;
    }
}

void QnCloudSystemsFinder::setCloudSystems(const QnCloudSystemList &systems)
{
    SystemsHash updatedSystems;
    for (const auto system : systems)
    {
        updatedSystems.insert(
            system.id,
            QnSystemDescription::createCloudSystem(system.id, system.name));
    }

    typedef QSet<QString> IdsSet;

    const auto newIds = updatedSystems.keys().toSet();
    IdsSet removedIds;

    {
        const QnMutexLocker lock(&m_mutex);

        const auto oldIds = m_systems.keys().toSet();
        const auto addedIds = IdsSet(newIds).subtract(oldIds);

        removedIds = IdsSet(oldIds).subtract(newIds);

        std::swap(m_systems, updatedSystems);
        for (const auto system : m_systems)
        {
            if (addedIds.contains(system->id()))
            {
                updateSystem(system->id());
                emit systemDiscovered(system);
            }
        }
    }

    for (const auto removedId : removedIds)
        emit systemLost(removedId);
}

void QnCloudSystemsFinder::onCloudError(QnCloudStatusWatcher::ErrorCode error)
{
    // TODO: #ynikitenkov handle errors. Now it is assumed that systems are reset
    // if any error automatically
}

void QnCloudSystemsFinder::updateSystem(const QString &systemId)
{
    using namespace nx::network;
    typedef std::vector<cloud::AddressResolver::TypedAddres> AddressVector;

    auto &resolver = nx::network::SocketGlobals::addressResolver();
    const QPointer<QnCloudSystemsFinder> guard(this);
    const auto resolvedHandler = [this, guard, systemId](const AddressVector &hosts)
    {
        if (!guard)
            return;

        {
            const QnMutexLocker lock(&m_mutex);
            const auto it = m_systems.find(systemId);
            if (it == m_systems.end())
                return;
        }

        for (const auto &host : hosts)
        {
            static const auto kModuleInformationTemplate
                = lit("http://%1:0/api/moduleInformation");
            const auto apiUrl = QUrl(kModuleInformationTemplate.arg(host.first.toString()));

            const QPointer<QnCloudSystemsFinder> guard(this);
            const auto onModuleInformationCompleted = [this, guard, systemId, host]
                (SystemError::ErrorCode errorCode, int httpCode, nx_http::BufferType buffer)
            {
                enum { kHttpSuccess = 200 };
                if (!guard || (errorCode != SystemError::noError)
                    || (httpCode != kHttpSuccess))
                {
                    return;
                }

                QnModuleInformation moduleInformation;
                if (!QJson::deserialize(buffer, &moduleInformation))
                    return;

                const QnMutexLocker lock(&m_mutex);
                const auto it = m_systems.find(systemId);
                if (it == m_systems.end())
                    return;

                const auto systemDescription = it.value();
                if (systemDescription->containsServer(moduleInformation.id))
                    systemDescription->updateServer(moduleInformation);
                else
                    systemDescription->addServer(moduleInformation
                        , static_cast<int>(host.second));

                systemDescription->setServerHost(moduleInformation.id
                    , host.first.toString());
            };

            nx_http::downloadFileAsync(apiUrl, onModuleInformationCompleted);
        }
    };

    const auto cloudHost = HostAddress(systemId);
    resolver.resolveDomain(cloudHost, resolvedHandler);
}

void QnCloudSystemsFinder::updateSystems()
{
    const QnMutexLocker lock(&m_mutex);
    for (const auto systemId : m_systems.keys())
        updateSystem(systemId);
}
