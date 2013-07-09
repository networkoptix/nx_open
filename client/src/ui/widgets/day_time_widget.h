#ifndef QN_DAY_TIME_WIDGET_H
#define QN_DAY_TIME_WIDGET_H

#include <QWidget>

class QLabel;
class QTableWidgetItem;

class QnDayTimeTableWidget;

class QnDayTimeWidget: public QWidget {
    Q_OBJECT
    typedef QWidget base_type;

public:
    QnDayTimeWidget(QWidget *parent = NULL);
    virtual ~QnDayTimeWidget();

signals:
    void timeClicked(const QTime &time);

private slots:
    void at_tableWidget_itemClicked(QTableWidgetItem *item);

private:
    QLabel *m_headerLabel;
    QnDayTimeTableWidget *m_tableWidget;
};

#endif // QN_DAY_TIME_WIDGET_H
