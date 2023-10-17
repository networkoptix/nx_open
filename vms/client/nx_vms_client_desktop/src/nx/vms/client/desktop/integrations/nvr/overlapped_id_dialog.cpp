// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_dialog.h"

#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <client/client_module.h>
#include <nx/vms/client/desktop/application_context.h>
#include <ui/workbench/workbench_context.h>

#include "overlapped_id.h"
#include "overlapped_id_dialog_model.h"
#include "overlapped_id_filter_model.h"
#include "overlapped_id_state.h"
#include "overlapped_id_store.h"

namespace nx::vms::client::desktop::integrations {

struct OverlappedIdDialog::Private: public QObject
{
    OverlappedIdDialog* q = nullptr;

    Private(OverlappedIdStore* store, OverlappedIdDialog* owner);

    OverlappedIdStore* store = nullptr;
    OverlappedIdDialogModel* model = nullptr;
    OverlappedIdFilterModel* filterModel = nullptr;

    QmlProperty<int> currentId;
    QmlProperty<QString> filter;
};

OverlappedIdDialog::Private::Private(OverlappedIdStore* store, OverlappedIdDialog* owner):
    q(owner),
    store(store),
    currentId(q->rootObjectHolder(), "currentId"),
    filter(q->rootObjectHolder(), "filter")
{
    model = new OverlappedIdDialogModel(owner);

    filterModel = new OverlappedIdFilterModel(owner);
    filterModel->setSourceModel(model);
}

OverlappedIdDialog::OverlappedIdDialog(OverlappedIdStore* store, QWidget* parent):
    QmlDialogWrapper(
        appContext()->qmlEngine(),
        QUrl("Nx/Dialogs/NvrOverlappedId/SelectOverlappedIdDialog.qml"),
        {},
        parent),
    QnWorkbenchContextAware(parent),
    d(new Private(store, this))
{
    connect(store, &OverlappedIdStore::stateChanged, this, &OverlappedIdDialog::update);
    update();

    QmlProperty<QObject*>(rootObjectHolder(), "model") = d->filterModel;

    d->currentId = store->state().currentId;
    d->currentId.connectNotifySignal(this, [this]{ updateCurrentId(); });

    d->filter.connectNotifySignal(this, [this]{ updateFilter();} );
}

OverlappedIdDialog::~OverlappedIdDialog()
{
}

void OverlappedIdDialog::update()
{
    d->model->setIdList(d->store->state().idList);
    d->currentId = d->store->state().currentId;
}

void OverlappedIdDialog::updateFilter()
{
    d->filterModel->setFilterWildcard(d->filter);
}

void OverlappedIdDialog::updateCurrentId()
{
    if (d->currentId != d->store->state().currentId)
        d->store->setCurrentId(d->currentId);
}

} // namespace nx::vms::client::desktop::integrations
