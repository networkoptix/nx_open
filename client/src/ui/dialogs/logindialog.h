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
    virtual ~LoginDialog();

public slots:
    virtual void accept() override;
    void reset();

protected:
    virtual void changeEvent(QEvent *event) override;

    QUrl currentUrl();

private slots:
    void testSettings();
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
