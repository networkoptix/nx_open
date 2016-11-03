
#include "ordered_systems_model.h"

#include <ui/models/systems_model.h>
#include <nx/utils/string.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <client/forgotten_systems_manager.h>

QnOrderedSystemsModel::QnOrderedSystemsModel(QObject* parent) :
    base_type(parent),
    m_weights(),
    m_unknownSystemsWeight(0.0)
{
    NX_ASSERT(qnSystemWeightsManager, "QnSystemWeightsManager is not available");
    if (!qnSystemWeightsManager)
        return;

    auto systemsModel = new QnSystemsModel(this);
    setSourceModel(systemsModel);

    connect(systemsModel, &QnSystemsModel::minimalVersionChanged,
        this, &QnOrderedSystemsModel::minimalVersionChanged);
    setDynamicSortFilter(true);
    sort(0);

    connect(qnSystemWeightsManager, &QnSystemsWeightsManager::weightsChanged,
        this, &QnOrderedSystemsModel::handleWeightsChanged);

    if (qnForgottenSystemsManager)
    {
        connect(qnForgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemsChanged,
            this, &QnOrderedSystemsModel::softInvalidate);
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
    static const QVector<int> kIdRoles =
        { QnSystemsModel::SystemIdRoleId, QnSystemsModel::LocalIdRoleId };

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

bool QnOrderedSystemsModel::lessThan(const QModelIndex& left,
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
    int sourceRow, const QModelIndex &sourceParent) const
{
    // Filters out offline non-cloud systems with last connection more than N (defined) days ago
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return true;

    if (index.data(QnSystemsModel::IsOnlineRoleId).toBool())
        return true;    //< Skips every online system

    const auto id = index.data(QnSystemsModel::SystemIdRoleId).toString();
    if (qnForgottenSystemsManager->isForgotten(id))
        return false;

    if (index.data(QnSystemsModel::IsCloudSystemRoleId).toBool())
        return true;    //< Skips offline cloud systems

    qreal weight = 0.0;
    if (!getWeightFromData(index, weight))
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

    softInvalidate();
}

void QnOrderedSystemsModel::softInvalidate()
{
    const auto source = sourceModel();
    if (!source)
        return;

    // Forces resort without tiles removal

    const auto sourceRowCount = source->rowCount();
    const auto start = source->index(0, 0);
    const auto end = source->index(sourceRowCount - 1, 0);

    emit source->dataChanged(start, end);
}
