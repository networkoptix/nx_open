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

    setModel(new DetectableObjectTypeModel(
        qnClientCoreModule->mainQmlEngine()->singletonInstance<TaxonomyManager*>(
            qmlTypeId("nx.vms.client.desktop.analytics", 1, 0, "TaxonomyManager")),
        this));
}

QString DetectableObjectTypeComboBox::selectedObjectTypeId() const
{
    return currentData(DetectableObjectTypeModel::IdRole).toString();
}

void DetectableObjectTypeComboBox::setSelectedObjectTypeId(const QString& value)
{
    const auto index = findData(value, DetectableObjectTypeModel::IdRole);
    if (index.isValid())
        setCurrentIndex(index);
}

} // namespace nx::vms::client::desktop
