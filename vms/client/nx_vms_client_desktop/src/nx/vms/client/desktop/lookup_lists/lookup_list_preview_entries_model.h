// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

/**
 * Raw presentation of first N rows from CSV, representing Lookup List Data.
 */
class NX_VMS_CLIENT_DESKTOP_API LookupListPreviewEntriesModel: public QAbstractTableModel
{
    Q_OBJECT
    using base_type = QAbstractTableModel;

public:
    Q_PROPERTY(LookupListEntriesModel* lookupListEntriesModel READ lookupListEntriesModel
        WRITE setLookupListEntriesModel NOTIFY lookupListEntriesModelChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged);
    Q_PROPERTY(QString doNotImportText READ doNotImportText CONSTANT)

    explicit LookupListPreviewEntriesModel(QAbstractTableModel* parent = nullptr);
    virtual ~LookupListPreviewEntriesModel() override;

    using PreviewRawData = std::vector<std::vector<QVariant>>;
    virtual QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE void setRawData(const PreviewRawData& rawData);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void headerIndexChanged(int index, const QString& translatedHeaderName);
    Q_INVOKABLE QMap<int, QString> columnIndexToAttribute();
    Q_INVOKABLE LookupListEntriesModel* lookupListEntriesModel();
    Q_INVOKABLE void setLookupListEntriesModel(LookupListEntriesModel* lookupListEntriesModel);
    Q_INVOKABLE QString doNotImportText() const;

signals:
    void lookupListEntriesModelChanged();
    void rowCountChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
