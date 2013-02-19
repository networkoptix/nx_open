#ifndef QN_DATABASE_MANAGEMENT_WIDGET_H
#define QN_DATABASE_MANAGEMENT_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtGui/QWidget>

#include <utils/common/request_param.h> /* For QnHTTPRawResponse */
#include <utils/common/connective.h>


struct QnHTTPRawResponse;

namespace Ui {
    class DatabaseManagementWidget;
}

// TODO: #qt5 replace with proper functor
class QnDatabaseManagementWidgetReplyProcessor: public QObject {
    Q_OBJECT;
public:
    QnDatabaseManagementWidgetReplyProcessor(QObject *parent = NULL): QObject(parent), handle(-1) {}

    QnHTTPRawResponse response;
    int handle;

signals:
    void activated();
    void activated(const QnHTTPRawResponse &response, int handle);

public slots:
    void activate(const QnHTTPRawResponse &response, int handle) {
        this->response = response;
        this->handle = handle;

        emit activated();
        emit activated(response, handle);
    }
};


class QnDatabaseManagementWidget: public AdlConnective<QWidget> {
    Q_OBJECT

    typedef AdlConnective<QWidget> base_type;

public:
    QnDatabaseManagementWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnDatabaseManagementWidget();

private slots:
    void at_backupButton_clicked();
    void at_restoreButton_clicked();

private:
    QScopedPointer<Ui::DatabaseManagementWidget> ui;
};


#endif // QN_DATABASE_MANAGEMENT_WIDGET_H
