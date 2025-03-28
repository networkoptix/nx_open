// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integrations_dialog.h"

#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

IntegrationsDialog::IntegrationsDialog(QWidget* parent):
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/IntegrationsManagement/IntegrationsDialog.qml"),
        /*initialProperties*/ {},
        parent),
    CurrentSystemContextAware(parent),
    store(std::make_unique<AnalyticsSettingsStore>(parent))
{
    store->updateEngines();
    rootObjectHolder()->object()->setProperty("store", QVariant::fromValue(store.get()));
}

void IntegrationsDialog::setCurrentTab(Tab tab)
{
    QMetaObject::invokeMethod(
        rootObjectHolder()->object(), "setCurrentTab", Q_ARG(QVariant, QVariant::fromValue(tab)));
}

void IntegrationsDialog::registerQmlType()
{
    qmlRegisterType<IntegrationsDialog>("nx.vms.client.desktop", 1, 0, "IntegrationsDialog");
}

} // namespace nx::vms::client::desktop
