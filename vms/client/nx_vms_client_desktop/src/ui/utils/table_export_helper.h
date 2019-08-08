#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QAbstractItemModel>

class QWidget;

class QnTableExportHelper
{
    Q_DECLARE_TR_FUNCTIONS(QnTableExportHelper)
public:
    static QModelIndexList getAllIndexes(QAbstractItemModel* model);

    static void exportToFile(
        const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        QWidget* parent = nullptr,
        const QString& caption = QString());

    static void copyToClipboard(
        const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes);

private:
    static void getGridData(
        const QAbstractItemModel* tableModel,
        const QModelIndexList& indexes,
        const QChar& textDelimiter,
        QString& textData,
        QString& htmlData);
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
