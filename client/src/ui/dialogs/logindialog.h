#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtGui/QDialog>

class QDataWidgetMapper;
class QStandardItemModel;

namespace Ui {
    class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = 0);
    ~LoginDialog();

public Q_SLOTS:
    virtual void accept();
    virtual void reset();

protected:
    void changeEvent(QEvent *event);

private Q_SLOTS:
    void testSettings();
    void updateStoredConnections();
    void currentIndexChanged(int index);
    void configureStoredConnections();
    void testResults(int status, const QByteArray &data, int requstHandle);

private:
    Q_DISABLE_COPY(LoginDialog)

    QUrl currentUrl();

    QScopedPointer<Ui::LoginDialog> ui;

    QStandardItemModel *m_connectionsModel;

    QDataWidgetMapper *m_dataWidgetMapper;
};

#endif // LOGINDIALOG_H
