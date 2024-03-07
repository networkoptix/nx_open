// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detectable_object_type_combo_box.h"

#include <QtQml/QQmlEngine>

#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/event_rules/models/detectable_object_type_model.h>

namespace nx::vms::client::desktop {

DetectableObjectTypeComboBox::DetectableObjectTypeComboBox(QWidget* parent):
    base_type(parent)
{
    using nx::vms::client::desktop::analytics::TaxonomyManager;

    const auto taxonomyManager =
        qnClientCoreModule->mainQmlEngine()->singletonInstance<TaxonomyManager*>(
            qmlTypeId("nx.vms.client.desktop.analytics", 1, 0, "TaxonomyManager"));

    const auto filterModel = taxonomyManager->createFilterModel(this);

    setModel(new DetectableObjectTypeModel(filterModel, this));
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
}

void DetectableObjectTypeComboBox::setDevices(const QnUuidSet& devices)
{
    if (auto objectTypeModel = dynamic_cast<DetectableObjectTypeModel*>(model()))
    {
        objectTypeModel->sourceModel()->setSelectedDevices(
            std::set<QnUuid>{devices.begin(), devices.end()});
    }
}

} // namespace nx::vms::client::desktop
