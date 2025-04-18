// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detectable_object_type_combo_box.h"

#include <QtQml/QQmlEngine>

#include <client/client_module.h>
#include <common/common_module.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/event_rules/models/detectable_object_type_model.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

DetectableObjectTypeComboBox::DetectableObjectTypeComboBox(QWidget* parent):
    base_type(parent)
{
    using nx::vms::client::core::analytics::TaxonomyManager;

    const auto taxonomyManager = appContext()->currentSystemContext()->taxonomyManager();
    const auto filterModel = taxonomyManager->createFilterModel(this);

    setModel(new DetectableObjectTypeModel(filterModel, this));
    setCurrentIndex(-1);
}

QStringList DetectableObjectTypeComboBox::selectedObjectTypeIds() const
{
    return currentData(DetectableObjectTypeModel::IdsRole).toStringList();
}

void DetectableObjectTypeComboBox::setSelectedObjectTypeIds(const QStringList& objectTypeIds)
{
    const auto index = findData(
        objectTypeIds,
        DetectableObjectTypeModel::IdsRole,
        Qt::MatchRecursive | Qt::MatchExactly | Qt::MatchCaseSensitive);

    if (index.isValid())
        setCurrentIndex(index);
    else
        setCurrentIndex(-1);
}

QString DetectableObjectTypeComboBox::selectedMainObjectTypeId() const
{
    return currentData(DetectableObjectTypeModel::MainIdRole).toString();
}

void DetectableObjectTypeComboBox::setSelectedMainObjectTypeId(const QString& objectTypeId)
{
    const auto index = findData(
        objectTypeId,
        DetectableObjectTypeModel::MainIdRole,
        Qt::MatchRecursive | Qt::MatchExactly | Qt::MatchCaseSensitive);

    if (index.isValid())
        setCurrentIndex(index);
    else
        setCurrentIndex(-1);
}

void DetectableObjectTypeComboBox::setDevices(const UuidSet& devices)
{
    if (auto objectTypeModel = dynamic_cast<DetectableObjectTypeModel*>(model()))
    {
        objectTypeModel->sourceModel()->setSelectedDevices(
            std::set<nx::Uuid>{devices.begin(), devices.end()});
    }
}

} // namespace nx::vms::client::desktop
