// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_action_handler.h"

#include <chrono>
#include <deque>

#include <QtQml/QQmlEngine>

#include <api/server_rest_connection.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_lists_dialog.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

using nx::vms::api::LookupListData;
using nx::vms::api::LookupListDataList;

namespace nx::vms::client::desktop {

namespace {

LookupListDataList exampleData()
{
    LookupListDataList result;

    LookupListData e1;
    e1.id = QnUuid::createUuid();
    e1.name = "Numbers";
    e1.attributeNames.push_back("Number");
    e1.attributeNames.push_back("Color");
    e1.entries = {
        { {"Number", "12345"}, {"Color", "red"} },
        { {"Number", "67890"} },
        { {"Color", "blue"} }
    };
    result.push_back(e1);

    LookupListData e2;
    e2.id = QnUuid::createUuid();
    e2.name = "Values";
    e2.attributeNames.push_back("Value");
    e2.entries = {
        { {"Value", "0"} },
        { {"Value", "1"} },
        { {"Value", "2"} },
        { {"Value", "3"} },
        { {"Value", "4"} },
        { {"Value", "5"} },
        { {"Value", "6"} }
    };
    result.push_back(e2);

    return result;
}

} // namespace

struct LookupListActionHandler::Private
{
    LookupListActionHandler* const q;
    std::unique_ptr<LookupListsDialog> dialog;
    std::optional<rest::Handle> requestId;
    std::deque<LookupListData> saveQueue;

    void cancelRequest()
    {
        if (requestId)
            q->connectedServerApi()->cancelRequest(*requestId);
        requestId = {};
        saveQueue = {};
    }

    void handleError(const QString& text)
    {
        if (dialog)
            dialog->showError(text);
    }

    void saveList(LookupListData list)
    {
        auto callback = nx::utils::guarded(q,
            [this](bool success,
                rest::Handle requestId,
                const rest::ServerConnection::EmptyResponseType& /*response*/)
            {
                if (this->requestId != requestId)
                    return;

                this->requestId = std::nullopt;

                if (success)
                {
                    processSaveQueue();
                }
                else
                {
                    NX_WARNING(this, "Lookup List save request %1 failed", requestId);
                    handleError(tr("Network request failed"));
                    if (dialog)
                        dialog->setSaveResult(false);
                }
            });

        requestId = q->connectedServerApi()->putEmptyResult(
            nx::format("/rest/v3/lookupLists/%1").arg(list.id),
            {},
            QByteArray::fromStdString(nx::reflect::json::serialize(list)),
            callback,
            q->thread());
        NX_VERBOSE(this, "Send save list %1 request (%2)", list.id, *requestId);
    }

    void processSaveQueue()
    {
        if (saveQueue.empty())
        {
            if (dialog)
                dialog->setSaveResult(/*success*/ true);
            return;
        }

        saveList(std::move(saveQueue.front()));
        saveQueue.pop_front();
    }

    void saveData(LookupListDataList data)
    {
        for (auto list: data)
        {
            if (!NX_ASSERT(!list.id.isNull()))
                continue;

            if (list != q->lookupListManager()->lookupList(list.id))
                saveQueue.push_back(std::move(list));
        }
        processSaveQueue();
    }
};

LookupListActionHandler::LookupListActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private{.q = this})
{
    registerDebugAction(
        "Lists Lookup Dialog",
        [this](QnWorkbenchContext* context)
        {
            using namespace std::chrono;
            static constexpr auto kLoadDelay = 500ms;
            static constexpr auto kSaveDelay = 1000ms;

            static LookupListDataList sourceData = exampleData();

            appContext()->qmlEngine()->clearComponentCache();
            auto dialog = std::make_unique<LookupListsDialog>(
                context->systemContext(),
                context->mainWindowWidget());
            executeDelayedParented(
                [d = dialog.get()]() { d->setData(sourceData); }, kLoadDelay, dialog.get());

            connect(dialog.get(), &LookupListsDialog::saveRequested, this,
                [this, d = dialog.get()](LookupListDataList data)
                {
                    executeDelayedParented(
                        [data, d]
                        {
                            sourceData = data;
                            d->setSaveResult(true);
                        }, kSaveDelay, d);
                });

            dialog->exec();
        });

    connect(action(menu::OpenLookupListsDialogAction),
        &QAction::triggered,
        this,
        &LookupListActionHandler::openLookupListsDialog);

    connect(lookupListManager(), &common::LookupListManager::initialized, this,
        [this]()
        {
            if (d->dialog)
                d->dialog->setData(lookupListManager()->lookupLists());
        });
}

LookupListActionHandler::~LookupListActionHandler()
{
    d->cancelRequest();
}

void LookupListActionHandler::openLookupListsDialog()
{
    if (appContext()->qmlEngine()->baseUrl().isLocalFile())
        appContext()->qmlEngine()->clearComponentCache();

    d->dialog = std::make_unique<LookupListsDialog>(systemContext(), mainWindowWidget());
    if (lookupListManager()->isInitialized())
    {
        d->dialog->setData(lookupListManager()->lookupLists());
    }

    connect(d->dialog.get(), &LookupListsDialog::saveRequested, this,
        [this](LookupListDataList data) { d->saveData(std::move(data)); });

    d->dialog->exec();
    d->dialog.reset();
}

} // namespace nx::vms::client::desktop
