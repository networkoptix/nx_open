// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_lists_dialog.h"

#include <QtCore/QUrl>

#include <nx/vms/client/desktop/application_context.h>

#include "lookup_lists_dialog_store.h"

namespace nx::vms::client::desktop {

struct LookupListsDialog::Private
{
    LookupListsDialog* const q;
    std::unique_ptr<LookupListsDialogStore> store = std::make_unique<LookupListsDialogStore>();
};

LookupListsDialog::LookupListsDialog(QWidget* parent):
    base_type(
        appContext()->qmlEngine(),
        QUrl("Nx/LookupLists/LookupListsDialog.qml"),
        /*initialProperties*/ {},
        parent),
    d(new Private{.q = this})
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = this;
    QmlProperty<QObject*>(rootObjectHolder(), "store") = d->store.get();
}

LookupListsDialog::~LookupListsDialog()
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = nullptr;
    QmlProperty<QObject*>(rootObjectHolder(), "store") = nullptr;
}

void LookupListsDialog::setData(nx::vms::api::LookupListDataList data)
{
    d->store->loadData(std::move(data));
}

void LookupListsDialog::showError(const QString& text)
{
    QMetaObject::invokeMethod(window(), "showError", Q_ARG(QString, text));
}

void LookupListsDialog::setSaveResult(bool success)
{
    QmlProperty<bool>(window(), "saving") = false;

    if (success)
    {
        d->store->hasChanges = false;
        emit d->store->hasChangesChanged();

        if (QmlProperty<bool>(window(), "closing").value())
            accept();
    }
}

void LookupListsDialog::requestSave()
{
    emit saveRequested(d->store->data);
}

} // namespace nx::vms::client::desktop
