#ifndef __GRID_WIDGET_HELPER_H__
#define __GRID_WIDGET_HELPER_H__

#include <ui/workbench/workbench_context_aware.h>

class QTableView;


class QnGridWidgetHelper: public QnWorkbenchContextAware
{
public:
    QnGridWidgetHelper(QnWorkbenchContext *context);

    void exportToFile(QTableView* grid, const QString& caption);
    void copyToClipboard(QTableView* grid);
private:
    void getGridData(QTableView* grid, QString& textData, QString& htmlData, const QLatin1Char& textDelimiter);
};

#endif // __GRID_WIDGET_HELPER_H__
