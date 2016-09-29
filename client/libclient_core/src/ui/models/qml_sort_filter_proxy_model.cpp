#include "qml_sort_filter_proxy_model.h"

#include <ui/models/systems_model.h>
#include <nx/utils/string.h>
#include <nx/utils/log/assert.h>
#include <client_core/client_core_settings.h>

namespace {

typedef std::function<bool (const QModelIndex& left, const QModelIndex& right)>
    UnknownSystemsHandler;

template<typename WeightsHash>
bool genericLessThan(const QModelIndex& left,
    const QModelIndex& right,
    const WeightsHash& weights,
    const UnknownSystemsHandler& unknownSystemsHandler)
{
    const auto rightId = right.data(QnSystemsModel::SystemIdRoleId).toString();
    const auto itRightWeight = weights.find(rightId);

    const auto leftId = left.data(QnSystemsModel::SystemIdRoleId).toString();
    const auto itLeftWeight = weights.find(leftId);

    const bool rightIsUnknown = ((itRightWeight == weights.end()) || !right.isValid());
    const bool leftIsUnknown = ((itLeftWeight == weights.end()) || !left.isValid());

    if (rightIsUnknown && leftIsUnknown)
        return unknownSystemsHandler(left, right);

    if (rightIsUnknown || leftIsUnknown)
        return rightIsUnknown;

    // Sort by weight
    const auto leftWeight = itLeftWeight.value();
    const auto rightWeight = itRightWeight.value();
    return (leftWeight > rightWeight);  // System with greater weight will be placed at begin
}

}   // namespace

QnQmlSortFilterProxyModel::QnQmlSortFilterProxyModel(QObject* parent) :
    base_type(parent),
    m_cloudSystemWeight(),
    m_localSystemWeight()
{
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged,
        this, &QnQmlSortFilterProxyModel::handleCloudSystemsChanged);

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this]() { handleLocalConnectionsChanged();  });

    setSourceModel(new QnSystemsModel(this));

    handleCloudSystemsChanged();
    handleLocalConnectionsChanged();

    setDynamicSortFilter(true);
    sort(0);
}

bool QnQmlSortFilterProxyModel::lessThan(const QModelIndex& left,
    const QModelIndex& right) const
{
    const auto leftIsFactory = left.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    const auto rightIsFactory = right.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    if (leftIsFactory && rightIsFactory)
    {
        const auto leftSystemId = left.data(QnSystemsModel::SystemIdRoleId).toString();
        const auto rightSystemId = right.data(QnSystemsModel::SystemIdRoleId).toString();
        return (leftSystemId < rightSystemId);
    }

    if (leftIsFactory || rightIsFactory)
        return leftIsFactory;

    const auto localSystemsLess =
        [this](const QModelIndex& left, const QModelIndex& right) -> bool
        {
            static const auto compareNamesFunc =
                [](const QModelIndex& left, const QModelIndex& right) -> bool
            {
                const auto leftName = left.data(QnSystemsModel::SystemNameRoleId).toString();
                const auto rightName = right.data(QnSystemsModel::SystemNameRoleId).toString();
                return (nx::utils::naturalStringCompare(
                    leftName, rightName, Qt::CaseInsensitive) < 0);
            };

            return genericLessThan(left, right, m_localSystemWeight, compareNamesFunc);
        };

    return genericLessThan(left, right, m_cloudSystemWeight, localSystemsLess);
}

bool QnQmlSortFilterProxyModel::filterAcceptsRow(int row,
    const QModelIndex &parent) const
{
    if (!base_type::filterAcceptsRow(row, parent))
        return false;

    // Filters out offline non-cloud systems with last connection more than N (defined) days ago
    const auto index = sourceModel()->index(row, 0, parent);
    if (!index.isValid())
        return true;

    if (index.data(QnSystemsModel::IsOnlineRoleId).toBool()
        || index.data(QnSystemsModel::IsCloudSystemRoleId).toBool())
    {
        return true;   //< Skip online or cloud systems
    }

    const auto systemId = index.data(QnSystemsModel::SystemIdRoleId).toString();
    const auto itLocalSystemWeight = m_localSystemWeight.find(systemId);
    if (itLocalSystemWeight == m_localSystemWeight.end())
        return true;

    static const auto kMinWeight = 0.00001;
    return (itLocalSystemWeight.value() > kMinWeight);
}

void QnQmlSortFilterProxyModel::handleCloudSystemsChanged()
{
    const auto onlineSystems = qnCloudStatusWatcher->cloudSystems();
    const auto recentSystems = qnClientCoreSettings->recentCloudSystems();

    int index = onlineSystems.size() + recentSystems.size();

    auto getCloudSystemsOrder =
        [&index](const QnCloudSystemList& systems) -> IdWeightHash
        {
            IdWeightHash result;
            for (const auto system : systems)
                result.insert(system.id, --index);
            return result;
        };

    const auto newCloudSystemsOrder = getCloudSystemsOrder(onlineSystems).unite(
        getCloudSystemsOrder(recentSystems));

    if (newCloudSystemsOrder == m_cloudSystemWeight)
        return;

    m_cloudSystemWeight = newCloudSystemsOrder;
}

void QnQmlSortFilterProxyModel::handleLocalConnectionsChanged()
{
    const auto recentConnections = qnClientCoreSettings->recentLocalConnections();
    const auto newWeights = recentConnections.getWeights();
    if (newWeights == m_localSystemWeight)
        return;

    m_localSystemWeight = newWeights;
    invalidate();
}
