#include "ordered_systems_model.h"

#include <ui/models/systems_model.h>
#include <nx/utils/string.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <client/forgotten_systems_manager.h>

QnOrderedSystemsModel::QnOrderedSystemsModel(QObject* parent) :
    base_type(parent),
    m_source(new QnSystemsModel(this)),
    m_weights(),
    m_unknownSystemsWeight(0.0)
{
    NX_ASSERT(qnSystemWeightsManager, "QnSystemWeightsManager is not available");
    if (!qnSystemWeightsManager)
        return;

    setSourceModel(m_source);

    connect(m_source, &QnSystemsModel::minimalVersionChanged,
        this, &QnOrderedSystemsModel::minimalVersionChanged);

    connect(qnSystemWeightsManager, &QnSystemsWeightsManager::weightsChanged,
        this, &QnOrderedSystemsModel::handleWeightsChanged);

    // TODO: #ynikitenkov add triggering roles list here to optimize sorting/filtering model

    if (qnForgottenSystemsManager)
    {
        connect(qnForgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemRemoved,
            this, &QnOrderedSystemsModel::forceUpdate);
        connect(qnForgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemAdded,
            this, &QnOrderedSystemsModel::forceUpdate);
    }

    handleWeightsChanged();
}

bool QnOrderedSystemsModel::getWeightFromData(
    const QModelIndex& modelIndex,
    qreal& weight) const
{
    const auto fillWeightByRole =
        [this, modelIndex](int role, qreal& weight)
        {
            const auto id = modelIndex.data(role).toString();
            const auto itWeight = m_weights.find(id);
            if (itWeight == m_weights.end())
                return false;

            weight = itWeight.value().weight;
            return true;
        };

    // Searching for maximum weight
    static const QVector<int> kIdRoles{
        QnSystemsModel::SystemIdRoleId,
        QnSystemsModel::LocalIdRoleId};

    weight = 0.0;
    bool result = false;
    for (const auto& role : kIdRoles)
    {
        qreal foundWeight = 0.0;
        if (fillWeightByRole(role, foundWeight))
        {
            weight = std::max(weight, foundWeight);
            result = true;
        }
    }
    return result;
}

QString QnOrderedSystemsModel::minimalVersion() const
{
    if (auto systemsModel = dynamic_cast<QnSystemsModel*>(sourceModel()))
        return systemsModel->minimalVersion();
    return QString();
}

void QnOrderedSystemsModel::setMinimalVersion(const QString& minimalVersion)
{
    if (auto systemsModel = dynamic_cast<QnSystemsModel*>(sourceModel()))
        systemsModel->setMinimalVersion(minimalVersion);
}

qreal QnOrderedSystemsModel::getWeight(const QModelIndex& modelIndex) const
{
    qreal result = 0.0;
    return (getWeightFromData(modelIndex, result) ? result : m_unknownSystemsWeight);
}

bool QnOrderedSystemsModel::lessThan(
    const QModelIndex& left,
    const QModelIndex& right) const
{
    static const auto finalLess =
        [](const QModelIndex& left, const QModelIndex& right) -> bool
        {
            const auto leftSystemName = left.data(QnSystemsModel::SystemNameRoleId).toString();
            const auto rightSystemName = right.data(QnSystemsModel::SystemNameRoleId).toString();
            const auto compareResult = nx::utils::naturalStringCompare(
                leftSystemName, rightSystemName, Qt::CaseInsensitive);
            if (compareResult)
                return (compareResult < 0);

            const auto leftSystemId = left.data(QnSystemsModel::SystemIdRoleId).toString();
            const auto rightSystemId = right.data(QnSystemsModel::SystemIdRoleId).toString();
            return (leftSystemId < rightSystemId);
        };

    const auto leftIsFactory = left.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    const auto rightIsFactory = right.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    if (leftIsFactory && rightIsFactory)
        return finalLess(left, right);

    if (leftIsFactory || rightIsFactory)
        return leftIsFactory;

    // Sort by weight
    const auto leftWeight = getWeight(left);
    const auto rightWeight = getWeight(right);
    if (leftWeight == rightWeight)
        return finalLess(left, right);

    return (leftWeight > rightWeight);  // System with greater weight will be placed at begin
}

bool QnOrderedSystemsModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    const auto dataIndex = index(sourceRow);
    // Filters out offline non-cloud systems with last connection more than N (defined) days ago
    if (!dataIndex.isValid())
        return true;

    if (dataIndex.data(QnSystemsModel::IsConnectableRoleId).toBool())
        return true;    //< Skips every connectable system

    const auto id = dataIndex.data(QnSystemsModel::SystemIdRoleId).toString();
    if (qnForgottenSystemsManager && qnForgottenSystemsManager->isForgotten(id))
        return false;

    if (dataIndex.data(QnSystemsModel::IsCloudSystemRoleId).toBool())
        return true;    //< Skips offline cloud systems

    qreal weight = 0.0;
    if (!getWeightFromData(dataIndex, weight))
        return true;

    static const auto kMinWeight = 0.00001;
    return (weight > kMinWeight);
}

void QnOrderedSystemsModel::handleWeightsChanged()
{
    const bool sameUnknownWeight = qFuzzyEquals(m_unknownSystemsWeight,
        qnSystemWeightsManager->unknownSystemsWeight());

    const auto newWeights = qnSystemWeightsManager->weights();
    if (sameUnknownWeight && (newWeights == m_weights))
        return;

    m_unknownSystemsWeight = qnSystemWeightsManager->unknownSystemsWeight();
    m_weights = qnSystemWeightsManager->weights();

    forceUpdate();
}
