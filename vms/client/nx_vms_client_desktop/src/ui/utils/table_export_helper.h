#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QModelIndexList>

class QAbstractItemView;
class QWidget;

class QnTableExportHelper
{
    Q_DECLARE_TR_FUNCTIONS(QnTableExportHelper)
public:
    static void exportToFile(QAbstractItemView* table, bool onlySelected = true, QWidget* parent = nullptr, const QString& caption = QString());
    static void copyToClipboard(QAbstractItemView* table, bool onlySelected = true);

private:
    static void getGridData(QAbstractItemView* table, bool onlySelected, const QChar& textDelimiter, QString* textData, QString* htmlData);
    static QModelIndexList getAllIndexes(QAbstractItemModel* model);
};
