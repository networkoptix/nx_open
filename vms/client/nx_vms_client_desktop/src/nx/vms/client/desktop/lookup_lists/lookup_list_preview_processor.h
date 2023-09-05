// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QAbstractTableModel>

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

class LookupListModel;

/**
 * Raw presentation of first N rows from CSV.
 */
class LookupPreviewEntriesModel: public QAbstractTableModel
{
    Q_OBJECT
    using base_type = QAbstractTableModel;

public:
    explicit LookupPreviewEntriesModel(QAbstractTableModel* parent = nullptr);
    virtual ~LookupPreviewEntriesModel() override;

    using PreviewRawData = std::vector<std::vector<QVariant>>;
    virtual QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE void setHeaderRowData(LookupListEntriesModel* model);
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE void setRawData(const PreviewRawData& rawData);
    Q_INVOKABLE void reset();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * Processor of input csv/tsv file for preview, used in QML.
 * TBD: processing of changing of attributes type per column.
 */
class LookupListPreviewProcessor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    Q_PROPERTY(int rowsNumber READ rowsNumber WRITE setRowsNumber
        NOTIFY rowsNumberChanged REQUIRED);
    Q_PROPERTY(QString separator READ separator WRITE setSeparator NOTIFY separatorChanged);
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged);
    Q_PROPERTY(bool dataHasHeaderRow READ dataHasHeaderRow WRITE setDataHasHeaderRow
        NOTIFY dataHasHeaderRowChanged);

    explicit LookupListPreviewProcessor(QObject* parent = nullptr);
    virtual ~LookupListPreviewProcessor() override;
    Q_INVOKABLE void setImportFilePathFromDialog();
    Q_INVOKABLE bool buildTablePreview(LookupPreviewEntriesModel* model,
        const QString& filePath,
        const QString& separator,
        bool hasHeader);

    Q_INVOKABLE void reset(LookupPreviewEntriesModel* model);

    void setRowsNumber(int rowsNumber);
    void setSeparator(const QString& separator);
    void setFilePath(const QString& filePath);
    void setDataHasHeaderRow(bool dataHasHeaderRow);

    int rowsNumber();
    QString separator();
    QString filePath();
    bool dataHasHeaderRow();
signals:
    void rowsNumberChanged(int rowsNumber);
    void separatorChanged(const QString& separator);
    void filePathChanged(const QString& filePath);
    void dataHasHeaderRowChanged(bool dataHasHeaderRow);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
