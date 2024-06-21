// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>

class QWidget;

class QnTableExportHelper
{
    Q_DECLARE_TR_FUNCTIONS(QnTableExportHelper)
public:
    using FilterFunction = std::function<bool(const QModelIndex&)>;
    static QModelIndexList getAllIndexes(QAbstractItemModel* model);
    static QModelIndexList getFilteredIndexes(QAbstractItemModel* model, FilterFunction filter);

    static void exportToFile(
        const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        QWidget* parent = nullptr,
        const QString& caption = QString(),
        int dataRole = Qt::DisplayRole);

    static void copyToClipboard(
        const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        int dataRole = Qt::DisplayRole);

    static QString getExportFilePathFromDialog(
        const QString& fileFilters, QWidget* parent = nullptr, const QString& caption = QString());

    static void exportToStreamCsv(const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        QTextStream& outputCsv,
        int dataRole = Qt::DisplayRole);

private:
    static void getGridHtmlCsvData(const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        const QChar& textDelimiter,
        QString& textData,
        QString& htmlData,
        int dataRole = Qt::DisplayRole);

    static QString getGridCsvData(const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        const QChar& textDelimiter,
        int dataRole = Qt::DisplayRole);
    static QString getGridHtmlData(const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        int dataRole = Qt::DisplayRole);
};

class QnTableExportCompositeModel: public QAbstractItemModel
{
public:
    QnTableExportCompositeModel(QList<QAbstractItemModel*> models, QObject* parent = nullptr);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

private:
    std::pair<int, int> calculateSubTableRow(int row) const;
    QList<QAbstractItemModel*> m_models;
};
