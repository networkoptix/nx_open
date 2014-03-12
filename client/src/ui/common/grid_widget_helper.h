#ifndef __GRID_WIDGET_HELPER_H__
#define __GRID_WIDGET_HELPER_H__

#include <QtCore/QCoreApplication>

class QTableView;
class QWidget;

// TODO: #Elric #TR rename into something saner

class QnGridWidgetHelper {
    Q_DECLARE_TR_FUNCTIONS(QnGridWidgetHelper)
public:
    static void exportToFile(QTableView *grid, QWidget *parent, const QString &caption);
    static void copyToClipboard(QTableView *grid);

private:
    static void getGridData(QTableView *grid, const QLatin1Char &textDelimiter, QString *textData, QString *htmlData);
};

#endif // __GRID_WIDGET_HELPER_H__
