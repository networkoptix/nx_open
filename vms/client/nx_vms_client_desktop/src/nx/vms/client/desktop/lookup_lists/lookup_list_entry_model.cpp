// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entry_model.h"

namespace nx::vms::client::desktop {

LookupListEntryModel::LookupListEntryModel(QObject* parent):
    base_type(parent)
{
}

LookupListEntryModel::~LookupListEntryModel()
{
}

int LookupListEntryModel::rowCount(const QModelIndex& parent) const
{
    return (int) m_attributes.size();
}

QVariant LookupListEntryModel::data(const QModelIndex& index, int role) const
{
    const QString key = attribute(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
            return key;

        case Qt::EditRole:
        {
            auto iter = m_entry.find(key);
            if (iter != m_entry.cend())
                return iter->second;

            return QString();
        }
    }

    return {};
}

bool LookupListEntryModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    switch (role)
    {
        case Qt::EditRole:
        {
            const QString key = attribute(index.row());
            if (!key.isEmpty())
            {
                if (value.toString().isEmpty())
                    m_entry.erase(key);
                else
                    m_entry[key] = value.toString();
                return true;
            }

            return false;
        }
    }

    return false;
}

QString LookupListEntryModel::attribute(int index) const
{
    if (NX_ASSERT(index >= 0 && index < m_attributes.size()))
        return m_attributes[index];
    return QString();
}

nx::vms::api::LookupListEntry LookupListEntryModel::entry() const
{
    return m_entry;
}

void LookupListEntryModel::resetData(
    std::vector<QString> attributes,
    nx::vms::api::LookupListEntry entry)
{
    beginResetModel();
    m_attributes = std::move(attributes);
    m_entry = std::move(entry);
    endResetModel();
}

} // namespace nx::vms::client::desktop
