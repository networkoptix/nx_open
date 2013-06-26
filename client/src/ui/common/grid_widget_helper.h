#ifndef __GRID_WIDGET_HELPER_H
#define __GRID_WIDGET_HELPER_H

#include "ui/workbench/workbench_context_aware.h"

class QTableView;

class QnGridWidgetHelper: public QnWorkbenchContextAware
{
public:
    QnGridWidgetHelper(QnWorkbenchContext *context);

    void exportGrid(QTableView* grid);
    void gridToClipboard(QTableView* grid);
private:
    void getGridData(QTableView* grid, QString& textData, QString& htmlData, const QLatin1Char& textDelimiter);
};

#endif // __GRID_WIDGET_HELPER_H
