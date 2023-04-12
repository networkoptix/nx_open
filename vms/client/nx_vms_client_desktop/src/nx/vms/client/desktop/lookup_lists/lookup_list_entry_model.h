// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/vms/api/data/lookup_list_data.h>

namespace nx::vms::client::desktop {

/** Attributes of a single Lookup List. */
class LookupListEntryModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    explicit LookupListEntryModel(QObject* parent = nullptr);
    virtual ~LookupListEntryModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);

    nx::vms::api::LookupListEntry entry() const;

    void resetData(std::vector<QString> attributes, nx::vms::api::LookupListEntry entry);

private:
    QString attribute(int index) const;

private:
    std::vector<QString> m_attributes;
    nx::vms::api::LookupListEntry m_entry;
};

} // namespace nx::vms::client::desktop
