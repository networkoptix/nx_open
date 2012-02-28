#ifndef CONNECTION_TESTING_DIALOG_H
#define CONNECTION_TESTING_DIALOG_H

#include <QtGui/QDialog>
#include <QtCore/QTimer>

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
    virtual ~ConnectionTestingDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

    void timeout();
    void testResults(int status, const QByteArray &data, const QByteArray &errorString, int requestHandle);

private:
    void testSettings();

private:
    Q_DISABLE_COPY(ConnectionTestingDialog);

    QScopedPointer<Ui::ConnectionTestingDialog> ui;
    QTimer m_timeoutTimer;
    QUrl m_url;
};

#endif // CONNECTION_TESTING_DIALOG_H
