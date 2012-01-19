#ifndef CONNECTION_TESTING_DIALOG_H
#define CONNECTION_TESTING_DIALOG_H

#include <QtGui/QDialog>
#include <QtCore/QTimer>

#include "api/AppServerConnection.h"

class QDataWidgetMapper;
class QStandardItemModel;

namespace Ui {
    class ConnectionTestingDialog;
}

class ConnectionTestingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionTestingDialog(QWidget *parent, QUrl url);
    ~ConnectionTestingDialog();

public Q_SLOTS:
    virtual void accept();
    virtual void reject();

    void timeout();
    void testResults(int status, const QByteArray &data, int requestHandle);

private:
    void testSettings();

private:
    Q_DISABLE_COPY(ConnectionTestingDialog)

    QTimer m_timeoutTimer;
    QUrl m_url;

    QScopedPointer<Ui::ConnectionTestingDialog> ui;

    QnAppServerConnectionPtr connection;
};

#endif // CONNECTION_TESTING_DIALOG_H
