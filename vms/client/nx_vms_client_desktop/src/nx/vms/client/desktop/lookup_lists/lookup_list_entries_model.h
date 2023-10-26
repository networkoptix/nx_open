// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/lookup_list_data.h>

#include "lookup_list_model.h"

namespace nx::vms::client::desktop {

/** Entries of a single Lookup List. */
class NX_VMS_CLIENT_DESKTOP_API LookupListEntriesModel: public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(LookupListModel* listModel
        READ listModel
        WRITE setListModel
        NOTIFY listModelChanged);

    Q_PROPERTY(int rowCount
        READ rowCount
        NOTIFY rowCountChanged);

    using base_type = QAbstractTableModel;

    enum DataRole
    {
        ObjectTypeIdRole = Qt::UserRole,
        AttributeNameRole
    };

public:
    explicit LookupListEntriesModel(QObject* parent = nullptr);
    virtual ~LookupListEntriesModel();

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole) override;

    virtual QHash<int, QByteArray> roleNames() const override;

    LookupListModel* listModel() const;
    void setListModel(LookupListModel* value);

    Q_INVOKABLE void addEntry(const QVariantMap& values);
    Q_INVOKABLE void deleteEntries(const QVector<int>& rows);
    void exportEntries(const QSet<int>& selectedRows, QTextStream& outputCsv);
    bool updateHeaders(const QStringList& headers);
    bool validate();
    Q_INVOKABLE void setFilter(const QString& searchText, int resultLimit);

signals:
    void listModelChanged(LookupListModel* listModel);
    void rowCountChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
