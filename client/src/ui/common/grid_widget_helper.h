#ifndef __GRID_WIDGET_HELPER_H__
#define __GRID_WIDGET_HELPER_H__

#include <QtCore/QLatin1Char>

#include <ui/workbench/workbench_context_aware.h>

class QTableView;


class QnGridWidgetHelper: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnGridWidgetHelper(QnWorkbenchContext *context); // TODO: QObject *parent

    void exportToFile(QTableView* grid, const QString& caption);
    void copyToClipboard(QTableView* grid);
private:
    void getGridData(QTableView* grid, QString& textData, QString& htmlData, const QLatin1Char& textDelimiter);
};

#endif // __GRID_WIDGET_HELPER_H__
