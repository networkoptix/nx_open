// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_lists_dialog_store.h"

namespace nx::vms::client::desktop {

LookupListsDialogStore::LookupListsDialogStore(QObject* parent):
    QObject(parent),
    m_listsModel(std::make_unique<LookupListsModel>()),
    m_selectedListModel(std::make_unique<LookupListModel>(this)),
    m_entryModel(std::make_unique<LookupListEntryModel>())
{
}

LookupListsDialogStore::~LookupListsDialogStore()
{
}

LookupListsModel* LookupListsDialogStore::listsModel() const
{
    return m_listsModel.get();
}

LookupListModel* LookupListsDialogStore::selectedListModel() const
{
    return m_selectedListModel.get();
}

LookupListEntryModel* LookupListsDialogStore::entryModel() const
{
    return m_entryModel.get();
}

void LookupListsDialogStore::loadData(nx::vms::api::LookupListDataList value)
{
    data = std::move(value);
    m_listsModel->resetData(data);

    loaded = true;
    emit loadedChanged();

    hasChanges = false;
    emit hasChangesChanged();

    selectList(data.empty() ? -1 : 0);
}

void LookupListsDialogStore::addNewGenericList(const QString& name, const QString& columnName)
{
    nx::vms::api::LookupListData added;
    added.id = QnUuid::createUuid();
    added.name = name;
    added.attributeNames.push_back(columnName);
    data.push_back(std::move(added));

    m_listsModel->addList(data.back());
    selectList(data.size() - 1);

    hasChanges = true;
    emit hasChangesChanged();
}

void LookupListsDialogStore::editCurrentGenericList(const QString& name, const QString& columnName)
{
    const bool indexIsValid = listIndex >= 0 && listIndex < data.size();
    if (!NX_ASSERT(indexIsValid))
        return;

    auto& list = data[listIndex];
    list.name = name;
    if (NX_ASSERT(!list.attributeNames.empty()))
    {
        const QString oldName = list.attributeNames[0];
        list.attributeNames[0] = columnName;
        for (auto& entry: list.entries)
        {
            auto iter = entry.find(oldName);
            if (iter != entry.end())
            {
                const QString value = iter->second;
                entry.erase(iter);
                entry[columnName] = value;
            }
        }
    }
    else
    {
        list.attributeNames.push_back(columnName);
    }
    m_selectedListModel->resetData(list);
    m_listsModel->updateList(list);

    hasChanges = true;
    emit hasChangesChanged();
}

void LookupListsDialogStore::deleteCurrentList()
{
    const bool indexIsValid = listIndex >= 0 && listIndex < data.size();
    if (!NX_ASSERT(indexIsValid))
        return;

    auto iter = data.begin();
    std::advance(iter, listIndex);
    m_listsModel->removeList(iter->id);
    data.erase(iter);
    const auto newIndex = listIndex > 0
        ? listIndex - 1
        : data.empty() ? -1 : 0;
    selectList(newIndex);
}

void LookupListsDialogStore::selectList(int index)
{
    listIndex = index;
    const bool indexIsValid = index >= 0 && index < data.size();

    if (indexIsValid)
    {
        const auto& list = data[index];
        m_selectedListModel->resetData(list);
        m_entryModel->resetData(list.attributeNames, {});
    }
    else
    {
        m_selectedListModel->resetData({});
        m_entryModel->resetData({}, {});
    }
    emit listIndexChanged();
}

void LookupListsDialogStore::addItem()
{
    const bool indexIsValid = listIndex >= 0 && listIndex < data.size();
    if (!NX_ASSERT(indexIsValid))
        return;

    m_selectedListModel->addEntry(m_entryModel->entry());
    data[listIndex].entries.push_back(m_entryModel->entry());
    m_entryModel->resetData(data[listIndex].attributeNames, {});

    hasChanges = true;
    emit hasChangesChanged();
}

} // namespace nx::vms::client::desktop
