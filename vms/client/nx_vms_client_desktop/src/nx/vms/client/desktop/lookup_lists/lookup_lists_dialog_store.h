// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>

#include "lookup_list_entry_model.h"
#include "lookup_list_model.h"
#include "lookup_lists_model.h"

namespace nx::vms::client::desktop {

class LookupListsDialogStore: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded MEMBER loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool hasChanges MEMBER hasChanges NOTIFY hasChangesChanged)
    Q_PROPERTY(int listIndex MEMBER listIndex NOTIFY listIndexChanged)
    Q_PROPERTY(LookupListModel* selectedListModel READ selectedListModel CONSTANT)
    Q_PROPERTY(LookupListsModel* listsModel READ listsModel CONSTANT)
    Q_PROPERTY(LookupListEntryModel* entryModel READ entryModel CONSTANT)

public:
    LookupListsDialogStore(QObject* parent = nullptr);
    virtual ~LookupListsDialogStore() override;

    LookupListsModel* listsModel() const;
    LookupListModel* selectedListModel() const;
    LookupListEntryModel* entryModel() const;

    void loadData(nx::vms::api::LookupListDataList value);
    Q_INVOKABLE void addNewGenericList(const QString& name, const QString& columnName);
    Q_INVOKABLE void editCurrentGenericList(const QString& name, const QString& columnName);
    Q_INVOKABLE void deleteCurrentList();
    Q_INVOKABLE void selectList(int index);
    Q_INVOKABLE void addItem();

signals:
    void loadedChanged();
    void hasChangesChanged();
    void listIndexChanged();

public:
    /** Whether actual data is loaded. */
    bool loaded = false;

    /** Whether there are unsaved changes. */
    bool hasChanges = false;

    /** Index of a currently selected list. */
    int listIndex = -1;

    /** Lists data. */
    nx::vms::api::LookupListDataList data;

private:
    /** Model of lists. */
    std::unique_ptr<LookupListsModel> m_listsModel;

    /** Model of currently selected list items. */
    std::unique_ptr<LookupListModel> m_selectedListModel;

    /** Model of a single entry of the selected list. */
    std::unique_ptr<LookupListEntryModel> m_entryModel;
};

} // namespace nx::vms::client::desktop
