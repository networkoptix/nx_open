// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view.h>

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

    Q_PROPERTY(core::analytics::taxonomy::StateView* taxonomy
        READ taxonomy
        WRITE setTaxonomy
        NOTIFY taxonomyChanged)

    using base_type = QAbstractTableModel;
public:
    enum DataRole
    {
        ObjectTypeIdRole = Qt::UserRole,
        AttributeNameRole,
        RawValueRole, //< Raw value of entry.
        // Combination of DisplayRole and RawValueRole.
        // Required for correct sorting of number values.
        // Will return empty QVariant for empty values, and display values for all other values.
        SortRole,
        ColorRGBHexValueRole
    };
    Q_ENUMS(DataRole);

    explicit LookupListEntriesModel(QObject* parent = nullptr);
    virtual ~LookupListEntriesModel();

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation = Qt::Orientation::Horizontal,
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
    bool isValidValue(const QString& value, const QString& attributeName);

    Q_INVOKABLE core::analytics::taxonomy::StateView* taxonomy();
    Q_INVOKABLE void setTaxonomy(core::analytics::taxonomy::StateView* taxonomy);
    int columnPosOfAttribute(const QString& attributeName);

    Q_INVOKABLE void removeIncorrectEntries();

    void update();

signals:
    void listModelChanged(LookupListModel* listModel);
    void rowCountChanged();
    void taxonomyChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
