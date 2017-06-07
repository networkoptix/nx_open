#include "ptz_preset_model.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/ptz/ptz_helpers.h>

namespace {

static constexpr int kIdRoleId = Qt::UserRole + 1;
static const auto kRoleNames = QHash<int, QByteArray>{
    {kIdRoleId, "id"}};

} // namespace

namespace nx {
namespace client {
namespace mobile {

struct PtzPresetModel::Private
{
    QnUuid uniqueResourceId;
    QnPtzPresetList presets;
};

PtzPresetModel::PtzPresetModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(this, &PtzPresetModel::resourceIdChanged, this,
        [this]()
        {
            const auto resource = qnResPool->getResourceById(d->uniqueResourceId);
            const auto controller = qnClientPtzPool->controller(resource);
            const auto presets = core::ptz::helpers::getSortedPresets(controller);

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

QString PtzPresetModel::resourceId() const
{
    return d->uniqueResourceId.toString();
}

void PtzPresetModel::setResourceId(const QString& value)
{
    const auto id = QnUuid::fromStringSafe(value);
    if (d->uniqueResourceId == id)
        return;

    d->uniqueResourceId = id;
    emit resourceIdChanged();
}

} // namespace mobile
} // namespace client
} // namespace nx

