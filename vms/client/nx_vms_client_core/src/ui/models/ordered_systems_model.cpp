#include "ordered_systems_model.h"

#include <QtCore/QRegularExpression>

#include <ui/models/systems_model.h>
#include <nx/utils/string.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <client/forgotten_systems_manager.h>

namespace {

using namespace nx::vms::client::core;

static const QSet<int> kSortingRoles = {
    QnSystemsModel::SystemNameRoleId,
    QnSystemsModel::SystemIdRoleId,
    QnSystemsModel::LocalIdRoleId,
    QnSystemsModel::IsFactorySystemRoleId};

bool getWeightFromData(
    const QnWeightsDataHash& weights,
    const QModelIndex& modelIndex,
    WeightData& weightData)
{
    const auto fillWeightByRole =
        [weights, modelIndex](int role, WeightData& weight)
        {
            const auto id = modelIndex.data(role).toString();
            const auto itWeight = weights.find(id);
            if (itWeight == weights.end())
                return false;

            weight = itWeight.value();
            return true;
        };

    // Searching for maximum weight
    static const QVector<int> kIdRoles{
        QnSystemsModel::SystemIdRoleId,
        QnSystemsModel::LocalIdRoleId};

    weightData = WeightData();

    bool result = false;
    for (const auto& role: kIdRoles)
    {
        WeightData foundWeight;
        if (fillWeightByRole(role, foundWeight))
        {
            weightData = weightData.weight > foundWeight.weight
                ? weightData
                : foundWeight;

            result = true;
        }
    }
    return result;
}

class FilterModel: public QnSortFilterListModel
{
    using base_type = QnSortFilterListModel;
public:
    FilterModel(QObject* parent = nullptr);

    QnSystemsModel* systemsModel();

    bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

    void setWeights(const QnWeightsDataHash& weights);

private:
    const QScopedPointer<QnSystemsModel> m_systemsModel;
    QnWeightsDataHash m_weights;
};

FilterModel::FilterModel(QObject* parent):
    base_type(parent),
    m_systemsModel(new QnSystemsModel())
{
    setSourceModel(m_systemsModel.data());
    setTriggeringRoles(kSortingRoles);
}

QnSystemsModel* FilterModel::systemsModel()
{
    return m_systemsModel.data();
}

void FilterModel::setWeights(const QnWeightsDataHash& weights)
{
    if (m_weights == weights)
        return;

    m_weights = weights;
    forceUpdate();
}

bool FilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& /* sourceParent */) const
{
    const auto dataIndex = sourceModel()->index(sourceRow, 0);
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

    WeightData weightData;
    if (!getWeightFromData(m_weights, dataIndex, weightData))
        return true;

    static const auto kMinWeight = 0.00001;
    return (weightData.weight > kMinWeight);
}

} // namespace

QnOrderedSystemsModel::QnOrderedSystemsModel(QObject* parent) :
    base_type(parent),
    m_weights(),
    m_unknownSystemsWeight(0.0)
{
    NX_ASSERT(qnSystemWeightsManager, "QnSystemWeightsManager is not available");
    if (!qnSystemWeightsManager)
        return;

    const auto wildcardFilteredModel = new FilterModel(this);
    setSourceModel(wildcardFilteredModel);

    connect(wildcardFilteredModel->systemsModel(), &QnSystemsModel::minimalVersionChanged,
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

    setTriggeringRoles(kSortingRoles);

    handleWeightsChanged();
}

QString QnOrderedSystemsModel::minimalVersion() const
{
    if (auto model = dynamic_cast<FilterModel*>(sourceModel()))
        return model->systemsModel()->minimalVersion();

    NX_ASSERT(false, "Wrong source model!");
    return QString();
}

void QnOrderedSystemsModel::setMinimalVersion(const QString& minimalVersion)
{
    if (auto model = dynamic_cast<FilterModel*>(sourceModel()))
        model->systemsModel()->setMinimalVersion(minimalVersion);
    else
        NX_ASSERT(false, "Wrong source model!");
}

void QnOrderedSystemsModel::forceUpdate()
{
    base_type::forceUpdate();
    if (auto model = dynamic_cast<FilterModel*>(sourceModel()))
        model->forceUpdate();
    else
        NX_ASSERT(false, "Wrong source model!");
}

WeightData QnOrderedSystemsModel::getWeight(const QModelIndex& modelIndex) const
{
    WeightData result;
    return getWeightFromData(m_weights, modelIndex, result)
        ? result
        : WeightData{QnUuid::createUuid(), m_unknownSystemsWeight, 0, false};
}

bool QnOrderedSystemsModel::lessThan(
    const QModelIndex& left,
    const QModelIndex& right) const
{
    // Current sorting order:
    //
    // 1) Newly created systems with 127.0.0.1 address then
    //        By case insesitive system name then by system id
    // 2) Systems with 127.0.0.1 address then
    //        By case insesitive system name then by system id
    // 3) Newly created systems then
    //        By case insesitive system name then by system id
    // 4) By weight then
    //        Near zero weights are sorted by last connection time then
    //        By case insesitive system name then by system id

    static const auto isLessSystemNameThenSystemId =
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

    static const QRegularExpression localhostRegExp(
        "\\b(127\\.0\\.0\\.1|localhost)\\b",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DontCaptureOption);

    const auto leftIsLocalhost = left.data(QnSystemsModel::SearchRoleId).toString().contains(localhostRegExp);
    const auto rightIsLocalhost = right.data(QnSystemsModel::SearchRoleId).toString().contains(localhostRegExp);

    const auto leftIsFactory = left.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    const auto rightIsFactory = right.data(QnSystemsModel::IsFactorySystemRoleId).toBool();

    // Localhost systems go first
    if (leftIsLocalhost && rightIsLocalhost)
    {
        // Newly created systems on localhost go first
        if (leftIsFactory && rightIsFactory)
            return isLessSystemNameThenSystemId(left, right);

        if (leftIsFactory || rightIsFactory)
            return leftIsFactory;

        return isLessSystemNameThenSystemId(left, right);
    }

    if (leftIsLocalhost || rightIsLocalhost)
        return leftIsLocalhost;

    // Newly created systems go second
    if (leftIsFactory && rightIsFactory)
        return isLessSystemNameThenSystemId(left, right);

    if (leftIsFactory || rightIsFactory)
        return leftIsFactory;

    // The rest are sorted by weight
    const auto leftData = getWeight(left);
    const auto rightData = getWeight(right);

    if (leftData.weight == rightData.weight)
    {
        // Equal near zero weights are sorted by last connection time
        if (qFuzzyIsNull(leftData.weight) &&
            leftData.lastConnectedUtcMs != rightData.lastConnectedUtcMs)
        {
            return leftData.lastConnectedUtcMs < rightData.lastConnectedUtcMs;
        }

        return isLessSystemNameThenSystemId(left, right);
    }

    // System with greater weight will be placed at beginning
    return (leftData.weight > rightData.weight);
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

    const auto filterModel = static_cast<FilterModel*>(sourceModel());
    filterModel->setWeights(m_weights);

    forceUpdate();
}
