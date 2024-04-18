// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_action_handler.h"

#include <chrono>
#include <deque>
#include <unordered_set>

#include <QtQml/QQmlEngine>

#include <analytics/db/analytics_db_types.h>
#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_lists_dialog.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <ui/dialogs/common/message_box.h>
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
    e1.id = nx::Uuid::createUuid();
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
    e2.id = nx::Uuid::createUuid();
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

constexpr auto kMaxRequestAttemptCount = 5;

} // namespace

struct LookupListActionHandler::Private
{
    using SuccessHandler = std::function<void()>;
    struct DataDescriptor
    {
        LookupListData data;
        SuccessHandler successHandler;
    };

    LookupListActionHandler* const q;
    std::unique_ptr<LookupListsDialog> dialog;
    std::optional<rest::Handle> requestId;
    std::deque<DataDescriptor> saveQueue;
    std::deque<Uuid> removeQueue;
    size_t requestAttemptsCount{0};

    void cancelRequest()
    {
        if (requestId)
            q->connectedServerApi()->cancelRequest(*requestId);

        requestId = {};
        saveQueue = {};
        removeQueue = {};
    }

    void handleError(const QString& text)
    {
        if (dialog)
            dialog->showError(text);
    }

    void saveList(DataDescriptor dataDescriptor)
    {
        auto callback = nx::utils::guarded(q,
            [this, successHandler = dataDescriptor.successHandler](bool success,
                rest::Handle requestId,
                const rest::ServerConnection::EmptyResponseType& /*response*/)
            {
                if (this->requestId != requestId)
                    return;

                if (success)
                {
                    this->requestId = std::nullopt;
                    requestAttemptsCount = 0;
                    saveQueue.pop_front();

                    successHandler();
                    processQueues();
                }
                else
                {
                    ++requestAttemptsCount;
                    if (requestAttemptsCount <= kMaxRequestAttemptCount)
                    {
                        processQueues();
                        return;
                    }

                    NX_WARNING(this, "Lookup List save request %1 failed", requestId);
                    this->requestId = std::nullopt;

                    handleError(tr("Network request failed"));
                    if (dialog)
                        dialog->setSaveResult(false);
                }
            });

        requestId = q->connectedServerApi()->putEmptyResult(
            nx::format("/rest/v3/lookupLists/%1").arg(dataDescriptor.data.id),
            {},
            QByteArray::fromStdString(nx::reflect::json::serialize(dataDescriptor.data)),
            callback,
            q->thread());

        NX_VERBOSE(this, "Send save list %1 request (%2)", dataDescriptor.data.id, *requestId);
    }

    void removeList(Uuid listId)
    {
        auto callback = nx::utils::guarded(q,
            [this](bool success,
                rest::Handle requestId,
                const rest::ServerConnection::EmptyResponseType& /*response*/)
            {
                if (this->requestId != requestId)
                    return;

                if (success)
                {
                    this->requestId = std::nullopt;
                    requestAttemptsCount = 0;
                    removeQueue.pop_front();
                    processQueues();
                }
                else
                {
                    ++requestAttemptsCount;
                    if (requestAttemptsCount <= kMaxRequestAttemptCount)
                    {
                        processQueues();
                        return;
                    }

                    NX_WARNING(this, "Lookup List remove request %1 failed", requestId);
                    this->requestId = std::nullopt;

                    handleError(tr("Network request failed"));
                    if (dialog)
                        dialog->setSaveResult(false);
                }
            });

        requestId = q->connectedServerApi()->deleteEmptyResult(
            nx::format("/rest/v3/lookupLists/%1").arg(listId),
            {},
            callback,
            q->thread());

        NX_VERBOSE(this, "Send remove list %1 request (%2)", listId, *requestId);
    }

    void processQueues()
    {
        if (!saveQueue.empty())
        {
            saveList(std::move(saveQueue.front()));
            return;
        }

        if (!removeQueue.empty())
        {
            removeList(removeQueue.front());
            return;
        }

        if (dialog)
            dialog->setSaveResult(/*success*/ true);
    }


    void saveData(const DataDescriptor& data)
    {
        saveQueue.push_back(data);
        requestAttemptsCount = 0;
        processQueues();
    }

    void saveData(LookupListDataList data)
    {
        UuidSet listsToRemove;
        for (auto& list: q->lookupListManager()->lookupLists())
            listsToRemove.insert(list.id);

        for (auto list: data)
        {
            if (!NX_ASSERT(!list.id.isNull()))
                continue;

            listsToRemove.remove(list.id);

            if (list != q->lookupListManager()->lookupList(list.id))
                saveQueue.push_back({std::move(list)});
        }

        removeQueue.assign(listsToRemove.cbegin(), listsToRemove.cend());

        requestAttemptsCount = 0;
        processQueues();
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

    connect(action(menu::AddEntryToLookupListAction),
        &QAction::triggered,
        this,
        &LookupListActionHandler::onAddEntryToLookupListAction);

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

void LookupListActionHandler::onAddEntryToLookupListAction()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto listId = parameters.argument<nx::Uuid>(Qn::ItemUuidRole);
    if (!NX_ASSERT(!listId.isNull(), "Invalid list id passed"))
        return;

    auto lookupList = lookupListManager()->lookupList(listId);
    if (!NX_ASSERT(!lookupList.id.isNull(), "Lookup List with provided id wasn't found"))
        return;

    const std::unordered_set attributesNamesSet(
        lookupList.attributeNames.begin(), lookupList.attributeNames.end());

    auto entryToAdd = parameters.argument<api::LookupListEntry>(Qn::LookupListEntryRole);
    // Remove attributes, not used in lookup list.
    std::erase_if(
        entryToAdd, [&](auto& entry) { return !attributesNamesSet.contains(entry.first); });

    if (!NX_ASSERT(!entryToAdd.empty(), "Lookup List Entry is not correct."))
        return;

    lookupList.entries.push_back(entryToAdd);
    lookupListManager()->addOrUpdate(lookupList);
    d->saveData({lookupList,
        [this]()
        { QnMessageBox::success(mainWindowWidget(), tr("Object was added to the List")); }});
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

    d->dialog->exec(Qt::ApplicationModal);
    d->dialog.reset();
}

} // namespace nx::vms::client::desktop
