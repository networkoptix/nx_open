#include "ptz_preset_model.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/resource_pool.h>

namespace {

static constexpr int kIdRoleId = Qt::UserRole + 1;
static const auto kRoleNames = QHash<int, QByteArray>{
    {kIdRoleId, "id"}};

QnPtzPresetList getPresets(const QnPtzControllerPtr& controller)
{
    if (!controller)
        return QnPtzPresetList();

    QnPtzPresetList presets;
    if (!controller->getPresets(&presets))
        return QnPtzPresetList();

    return presets;
}

} // namespace

namespace nx {
namespace client {
namespace mobile {

struct PtzPresetModel::Private
{
    QUuid uniqueResourceId;
    QnPtzPresetList presets;
};

PtzPresetModel::PtzPresetModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(this, &PtzPresetModel::uniqueResourceIdChanged, this,
        [this]()
        {
            const auto resource = qnResPool->getResourceById(d->uniqueResourceId);
            const auto controller = qnClientPtzPool->controller(resource);
            const auto presets = getPresets(controller);

            if (presets == d->presets)
                return;

            beginResetModel();
            d->presets = presets;
            endResetModel();
        });
}

PtzPresetModel::~PtzPresetModel()
{
}

QVariant PtzPresetModel::data(const QModelIndex& index, int role) const
{
    const auto row = index.row();
    if (!hasIndex(row, 0))
        return QVariant();

    const auto& preset = d->presets.at(row);
    switch (role)
    {
        case Qt::DisplayRole:
            return preset.name;
        case kIdRoleId:
            return preset.id;
        default:
            return QVariant();
    }

    return QVariant();
}

int PtzPresetModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        NX_EXPECT(false, "Parent should be null");
        return 0;
    }
    return d->presets.size();
}

QHash<int, QByteArray> PtzPresetModel::roleNames() const
{
    return base_type::roleNames().unite(kRoleNames);
}

QUuid PtzPresetModel::uniqueResourceId() const
{
    return d->uniqueResourceId;
}

void PtzPresetModel::setUniqueResourceId(const QUuid& value)
{
    if (d->uniqueResourceId == value)
        return;

    d->uniqueResourceId = value;
    emit uniqueResourceIdChanged();
}

} // namespace mobile
} // namespace client
} // namespace nx

