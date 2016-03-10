
#include "cloud_systems_finder.h"

#include <common/common_module.h>
#include <nx/network/socket_global.h>

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_mutex()
    , m_systems()
{
    const auto cloudWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    Q_ASSERT_X(cloudWatcher, Q_FUNC_INFO, "Cloud watcher is not ready");

    connect(cloudWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(cloudWatcher, &QnCloudStatusWatcher::cloudSystemsChanged
        , this, &QnCloudSystemsFinder::setCloudSystems);
    connect(cloudWatcher, &QnCloudStatusWatcher::error
        , this, &QnCloudSystemsFinder::onCloudError);
    connect(this, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnCloudSystemsFinder::onSystemDiscovered);
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
    // TODO: add sync
    SystemsHash updatedSystems;
    for (const auto system : systems)
    {
        updatedSystems.insert(system.id
            , QnSystemDescription::createCloudSystem(system.id, system.name));
    }

    typedef QSet<QnUuid> IdsSet;

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
                emit systemDiscovered(system);
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

void QnCloudSystemsFinder::onSystemDiscovered(const QnSystemDescriptionPtr &system)
{
    typedef std::vector<HostAddress> HostAddressVector;
    
    auto &resolver = nx::network::SocketGlobals::addressResolver();

    const QPointer<QnCloudSystemsFinder> guard(this);
    const auto id = system->id();
    const auto resolvedHandler = [this, guard, id](const HostAddressVector &hosts)
    {
        if (!guard)
            return;

        const auto it = m_systems.find(id);
        if (it == m_systems.end())
            return;

        int i = 0;
    };

    const auto cloudHost = HostAddress(id.toString());
    resolver.resolveDomain(cloudHost, resolvedHandler);
}


