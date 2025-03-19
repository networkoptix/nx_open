// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_model.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/ptz/helpers.h>
#include <nx/vms/client/core/system_context.h>

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
    QnVirtualCameraResourcePtr camera;
    QnPtzPresetList presets;
};

PtzPresetModel::PtzPresetModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(this, &PtzPresetModel::resourceChanged, this,
        [this]()
        {
            if (!d->camera)
                return;

            auto systemContext = nx::vms::client::core::SystemContext::fromResource(d->camera);
            const auto controller = systemContext->ptzControllerPool()->controller(d->camera);
            if (!NX_ASSERT(controller))
                return;

            const auto updatePresets =
                [this, rawController = controller.data()]()
                {
                    QnPtzPresetList presets;
                    // In case of error empty list will be returned.
                    nx::vms::client::core::ptz::helpers::getSortedPresets(rawController, presets);
                    if (presets == d->presets)
                        return;

                    beginResetModel();
                    d->presets = presets;
                    endResetModel();
                };

            updatePresets();

            connect(controller.data(), &QnAbstractPtzController::changed, this,
                [updatePresets](nx::vms::common::ptz::DataFields fields)
                {
                    if (fields.testFlag(nx::vms::common::ptz::DataField::presets))
                        updatePresets();
                });
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
        NX_ASSERT(false, "Parent should be null");
        return 0;
    }
    return d->presets.size();
}

QHash<int, QByteArray> PtzPresetModel::roleNames() const
{
    auto names = base_type::roleNames();
    names.insert(kRoleNames);
    return names;
}

QnResource* PtzPresetModel::rawResource() const
{
    return d->camera.get();
}

void PtzPresetModel::setRawResource(QnResource* value)
{
    const auto camera = value
        ? value->toSharedPointer().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr{};

    if (camera == d->camera)
        return;

    d->camera = camera;
    emit resourceChanged();
}

} // namespace mobile
} // namespace client
} // namespace nx
