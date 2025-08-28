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
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view.h>
#include <nx/vms/client/core/common/utils/text_utils.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_edit_dialog.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_lists_dialog.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

using nx::vms::api::LookupListData;
using nx::vms::api::LookupListDataList;

namespace nx::vms::client::desktop {

namespace {

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
    QPointer<LookupListsDialog> dialog;
    nx::Uuid lastSelectedListId;
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

    void handleError(const QString& title, const QString& description)
    {
        if (dialog)
            dialog->showError(title, description);
        else
            QnMessageBox::warning(q->mainWindowWidget(), title, description);
    }

    void processSaveQueue()
    {
        if (!NX_ASSERT(!saveQueue.empty()))
            return;

        auto callback = nx::utils::guarded(q,
            [this](bool success, rest::Handle requestId, rest::ServerConnection::ErrorOrEmpty result)
            {
                if (this->requestId != requestId)
                    return;

                if (success)
                {
                    this->requestId = std::nullopt;
                    requestAttemptsCount = 0;

                    if (NX_ASSERT(!saveQueue.empty()) && saveQueue.front().successHandler)
                        saveQueue.front().successHandler();

                    saveQueue.pop_front();

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

                    QString error;
                    if (!result)
                        error = result.error().errorString;

                    NX_WARNING(this, "Lookup List save request %1 failed - %2", requestId, error);
                    this->requestId = std::nullopt;

                    handleError(tr("Network request failed"), error);

                    if (dialog)
                        dialog->setSaveResult(false);
                }
            });

        const auto& dataDescriptor = saveQueue.front();
        requestId = q->connectedServerApi()->saveLookupList(dataDescriptor.data, callback, q->thread());

        NX_VERBOSE(this, "Send save list %1 request (%2)", dataDescriptor.data.id, *requestId);
    }

    void removeList(Uuid listId)
    {
        auto callback = nx::utils::guarded(q,
            [this](bool success,
                rest::Handle requestId,
                const rest::ServerConnection::ErrorOrEmpty& /*response*/)
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

                    handleError(tr("Network request failed"), tr("Lookup List remove request failed"));
                    if (dialog)
                        dialog->setSaveResult(false);
                }
            });

        requestId = q->connectedServerApi()->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
            /*helper*/ nullptr,
            nx::network::http::Method::delete_,
            nx::format("/rest/v4/lookupLists/%1").arg(listId),
            /*params*/ {},
            /*body*/ {},
            std::move(callback),
            q->thread());

        NX_VERBOSE(this, "Send remove list %1 request (%2)", listId, *requestId);
    }

    void processQueues()
    {
        if (!saveQueue.empty())
        {
            processSaveQueue();
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

    void updateDataDescriptorForAddEntryAction(DataDescriptor& descriptor, api::LookupListEntry entryToAdd)
    {
        const std::unordered_set attributesNamesSet(
            descriptor.data.attributeNames.begin(), descriptor.data.attributeNames.end());

        // Remove attributes, not used in lookup list.
        std::erase_if(
            entryToAdd, [&](auto& entry) { return !attributesNamesSet.contains(entry.first); });

        if (entryToAdd.empty())
        {
            handleError(tr("Could not add object to the List"),
                tr("An object doesn't have attributes that matches selected list"));
            return;
        }

        descriptor.data.entries.push_back(entryToAdd);
        descriptor.successHandler =
            [this, listName = descriptor.data.name, listId = descriptor.data.id]()
            {
                QnMessageBox messageBox(QnMessageBox::Icon::Success,
                    tr("Object was added to the List"),
                    {},
                    QDialogButtonBox::Ok,
                    QDialogButtonBox::NoButton,
                    q->mainWindowWidget());

                const auto elidedText = core::text_utils::elideTextRight(&messageBox,
                    tr("An object has been added to the \"%1\" successfully").arg(listName));

                messageBox.setInformativeText(elidedText);
                messageBox.exec();
                lastSelectedListId = listId;
            };
    }
};

LookupListActionHandler::LookupListActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private{.q = this})
{
    connect(action(menu::OpenEditLookupListsDialogAction),
        &QAction::triggered,
        this,
        &LookupListActionHandler::openLookupListEditDialog);

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

    Private::DataDescriptor descriptor {.data = lookupList};
    d->updateDataDescriptorForAddEntryAction(
        descriptor, parameters.argument<api::LookupListEntry>(Qn::LookupListEntryRole));
    d->saveData(descriptor);
    lookupListManager()->addOrUpdate(lookupList);
}

void LookupListActionHandler::openLookupListsDialog()
{
    if (appContext()->qmlEngine()->baseUrl().isLocalFile())
        appContext()->qmlEngine()->clearComponentCache();

    if (!d->dialog)
    {
        d->dialog = new LookupListsDialog{systemContext(), mainWindowWidget()};
        if (lookupListManager()->isInitialized())
        {
            d->dialog->setData(lookupListManager()->lookupLists());
            d->dialog->selectList(d->lastSelectedListId);
        }

        connect(d->dialog.get(), &LookupListsDialog::saveRequested, this,
            [this](LookupListDataList data) { d->saveData(std::move(data)); });

        connect(windowContext(), &WindowContext::beforeSystemChanged, this,
            [this]
            {
                d->lastSelectedListId = nx::Uuid();

                if (d->dialog)
                    d->dialog->deleteLater();
            });

        connect(d->dialog.get(), &LookupListsDialog::done, this,
            [this]
            {
                d->lastSelectedListId = d->dialog->selectedListId();

                // TODO: #vbutkevich add real-time data update. For now the dialog must be
                // recreated each time it is opened to ensure it reflects the current state.
                // Remove assert in destructor.
                d->dialog->deleteLater();
            });
    }

    d->dialog->open();
}

void LookupListActionHandler::openLookupListEditDialog()
{
    auto params = menu()->currentParameters(sender());

    if (appContext()->qmlEngine()->baseUrl().isLocalFile())
        appContext()->qmlEngine()->clearComponentCache();

    LookupListData data;
    data.objectTypeId = params.argument(Qn::AnalyticsObjectTypeIdRole).toString();
    data.id = nx::Uuid::createUuid();
    auto sourceModel = new LookupListModel(data);

    const auto parentWidget = utils::extractParentWidget(params, mainWindowWidget());
    auto dialog = new LookupListEditDialog(systemContext(),
        // If there is argument Qn::AnalyticsObjectTypeIdRole, open LookupListEditDialog with
        // specified type otherwise, any type of list is allowed.
        params.hasArgument(Qn::AnalyticsObjectTypeIdRole) ? sourceModel : nullptr,
        parentWidget);
    sourceModel->setParent(dialog);

    connect(
        dialog,
        &QmlDialogWrapper::done,
        this,
        [this, dialog, params = std::move(params)](bool accepted)
        {
            if (accepted)
            {
                Private::DataDescriptor descriptor{.data = dialog->getLookupListData()};
                if (params.hasArgument(Qn::LookupListEntryRole))
                {
                    d->updateDataDescriptorForAddEntryAction(
                        descriptor, params.argument<api::LookupListEntry>(Qn::LookupListEntryRole));
                }

                d->saveData(descriptor);
                systemContext()->lookupListManager()->addOrUpdate(descriptor.data);
                if (d->dialog)
                    d->dialog->appendData({descriptor.data});
            }

            dialog->deleteLater();
        });

    dialog->setTransientParent(parentWidget);
    dialog->open();
}

} // namespace nx::vms::client::desktop
