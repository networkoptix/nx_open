
#include "cloud_systems_finder.h"

#include <common/common_module.h>

namespace
{
    typedef QSet<QnUuid> IdsSet;

    IdsSet getSystemIds(const QnAbstractSystemsFinder::SystemDescriptionList &systems)
    {
        IdsSet result;
        for (const auto system : systems)
            result.insert(system->id());
        return result;
    };

}

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_systems()
{
    const auto cloudWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    Q_ASSERT_X(cloudWatcher, Q_FUNC_INFO, "Cloud watcher is not ready");

    connect(cloudWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnCloudSystemsFinder::onCloudStatusChanged);
    connect(cloudWatcher, &QnCloudStatusWatcher::cloudSystemsChanged
        , this, &QnCloudSystemsFinder::onCloudSystemsChanged);
    connect(cloudWatcher, &QnCloudStatusWatcher::error
        , this, &QnCloudSystemsFinder::onCloudError);
}

QnCloudSystemsFinder::~QnCloudSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnCloudSystemsFinder::systems() const
{
    return m_systems;
}

void QnCloudSystemsFinder::onCloudStatusChanged(QnCloudStatusWatcher::Status status)
{
    switch (status)
    {
    case QnCloudStatusWatcher::Online:
        break;  // TODO: handle if it is needed

    case QnCloudStatusWatcher::LoggedOut:
    case QnCloudStatusWatcher::Unauthorized:
        break;  // TODO: handle if it is needed

    case QnCloudStatusWatcher::Offline:
        break;  // TODO: #ynikitenkov add offline systems handling
    }
}

void QnCloudSystemsFinder::onCloudSystemsChanged(const QnCloudSystemList &systems)
{
    SystemDescriptionList updatedSystems;
    for (const auto system : systems)
    {
        updatedSystems.append(QnSystemDescription::createCloudSystem(
            system.id, system.name));
    }

    const auto oldIds = getSystemIds(m_systems);
    const auto newIds = getSystemIds(m_systems);

    const auto removedIds = IdsSet(oldIds).subtract(newIds);
    const auto addedIds = IdsSet(newIds).subtract(oldIds);

    std::swap(m_systems, updatedSystems);
    
    for (const auto removedId : removedIds)
        emit systemLost(removedId);

    for (const auto system : m_systems)
    {
        if (addedIds.contains(system->id()))
            emit systemDiscovered(system);
    }
}

void QnCloudSystemsFinder::onCloudError(QnCloudStatusWatcher::ErrorCode error)
{
    // TODO: #ynikitenkov handle errors. Now it is assumed that systems are reset 
    // if any error automatically
}

