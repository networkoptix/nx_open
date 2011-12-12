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
    void updateStoredConnections();
    void currentIndexChanged(int index);
    void configureStoredConnections();

private:
    Q_DISABLE_COPY(LoginDialog)

    QScopedPointer<Ui::LoginDialog> ui;

    QStandardItemModel *m_connectionsModel;

    QDataWidgetMapper *m_dataWidgetMapper;
};

#endif // LOGINDIALOG_H
