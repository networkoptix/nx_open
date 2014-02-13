#ifndef __GRID_WIDGET_HELPER_H__
#define __GRID_WIDGET_HELPER_H__

#include <QtCore/QLatin1Char>

class QTableView;
class QWidget;

class QnGridWidgetHelper: public QObject {
    Q_OBJECT
public:
    QnGridWidgetHelper(QWidget *widget = NULL);

    void exportToFile(QTableView* grid, const QString& caption);
    void copyToClipboard(QTableView* grid);
private:
    void getGridData(QTableView* grid, QString& textData, QString& htmlData, const QLatin1Char& textDelimiter);

    QWidget* m_widget;
};

#endif // __GRID_WIDGET_HELPER_H__
