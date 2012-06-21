#ifndef CONNECTION_TESTING_DIALOG_H
#define CONNECTION_TESTING_DIALOG_H

#include <QtGui/QDialog>
#include <QtCore/QTimer>

#include "api/app_server_connection.h"

class QDataWidgetMapper;
class QStandardItemModel;

namespace Ui {
    class ConnectionTestingDialog;
}

class QnConnectionTestingDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnConnectionTestingDialog(const QUrl &url, QWidget *parent = NULL);
    virtual ~QnConnectionTestingDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

    void timeout();

	void oldHttpTestResults(int status ,QByteArray errorString,QByteArray data, int handle);
    void testResults(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int requestHandle);

private:
    void testSettings();

private:
    Q_DISABLE_COPY(QnConnectionTestingDialog);

    QScopedPointer<Ui::ConnectionTestingDialog> ui;
    QTimer m_timeoutTimer;
    QUrl m_url;
    QnAppServerConnectionPtr m_connection;
};

#endif // CONNECTION_TESTING_DIALOG_H
