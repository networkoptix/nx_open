// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_lists_dialog.h"

#include <QtCore/QUrl>
#include <QtQml/QtQml>

#include <client/client_globals.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>

#include "lookup_list_entries_model.h"
#include "lookup_list_entries_sort_filter_proxy_model.h"
#include "lookup_list_export_processor.h"
#include "lookup_list_import_processor.h"
#include "lookup_list_preview_processor.h"

namespace nx::vms::client::desktop {

void LookupListsDialog::registerQmlTypes()
{
    qmlRegisterType<LookupListModel>("nx.vms.client.desktop", 1, 0, "LookupListModel");
    qmlRegisterType<LookupListEntriesModel>(
        "nx.vms.client.desktop", 1, 0, "LookupListEntriesModel");
    qmlRegisterType<LookupListExportProcessor>(
        "nx.vms.client.desktop", 1, 0, "LookupListExportProcessor");
    qmlRegisterSingletonInstance("nx.vms.client.desktop",
        1,
        0,
        "LookupListPreviewHelper",
        LookupListPreviewHelper::instance());
    qmlRegisterType<LookupListPreviewProcessor>(
        "nx.vms.client.desktop", 1, 0, "LookupListPreviewProcessor");
    qmlRegisterType<LookupListImportEntriesModel>(
        "nx.vms.client.desktop", 1, 0, "LookupListImportEntriesModel");
    qmlRegisterType<LookupListImportProcessor>(
        "nx.vms.client.desktop", 1, 0, "LookupListImportProcessor");
    qmlRegisterType<LookupListEntriesSortFilterProxyModel>(
        "nx.vms.client.desktop", 1, 0, "LookupListEntriesSortFilterProxyModel");
}

LookupListsDialog::LookupListsDialog(SystemContext* systemContext, QWidget* parent):
    base_type(
        appContext()->qmlEngine(),
        QUrl("Nx/LookupLists/LookupListsDialog.qml"),
        /*initialProperties*/ { {"systemContext", QVariant::fromValue(systemContext)} },
        parent),
    SystemContextAware(systemContext)
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = this;
}

LookupListsDialog::~LookupListsDialog()
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = nullptr;
}

void LookupListsDialog::setData(nx::vms::api::LookupListDataList data)
{
    QList<LookupListModel*> modelList;
    for (const auto& elem: data)
        modelList.push_back(new LookupListModel(elem, this));
    emit loadCompleted(modelList);
}

void LookupListsDialog::appendData(nx::vms::api::LookupListDataList data)
{
    QList<LookupListModel*> modelList;
    for (const auto& elem: data)
        modelList.push_back(new LookupListModel(elem, this));
    emit appendData(modelList);
}

Q_INVOKABLE void LookupListsDialog::createListRequested()
{
    appContext()->mainWindowContext()->menu()->triggerForced(menu::OpenEditLookupListsDialogAction,
        menu::Parameters().withArgument(Qn::ParentWidgetRole, QPointer(window())));
}

void LookupListsDialog::showError(const QString& title, const QString& description)
{
    QMetaObject::invokeMethod(
        window(), "showError", Q_ARG(QString, title), Q_ARG(QString, description));
}

void LookupListsDialog::setSaveResult(bool success)
{
    QmlProperty<bool>(window(), "saving") = false;
    if (success)
    {
        if (QmlProperty<bool>(window(), "closing").value())
            accept();
    }
    else
    {
        // Allow to run save request once more.
        QmlProperty<bool>(window(), "hasChanges") = true;
    }
}

void LookupListsDialog::save(QList<LookupListModel*> data)
{
    nx::vms::api::LookupListDataList result;
    for (auto list: data)
        result.push_back(list->rawData());

    emit saveRequested(result);
}

void LookupListsDialog::selectList(const nx::Uuid& listId)
{
    QMetaObject::invokeMethod(
        window(), "selectList", Q_ARG(QVariant, QVariant::fromValue(listId)));
}

nx::Uuid LookupListsDialog::selectedListId() const
{
    const auto currentList = QmlProperty<LookupListModel*>(window(), "currentList").value();
    return currentList ? currentList->rawData().id : nx::Uuid();
}

} // namespace nx::vms::client::desktop
