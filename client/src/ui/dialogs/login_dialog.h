#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtGui/QDialog>

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;

namespace Ui {
    class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    virtual ~LoginDialog();

public slots:
    virtual void accept() override;
    void reset();

protected:
    virtual void changeEvent(QEvent *event) override;

    QUrl currentUrl();

    void updateStoredConnections();

private slots:
    void updateAcceptibility();

    void at_configureConnectionsButton_clicked();
    void at_testButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(int index);

private:
    Q_DISABLE_COPY(LoginDialog)

    QScopedPointer<Ui::LoginDialog> ui;
    QWeakPointer<QnWorkbenchContext> m_context;
    QStandardItemModel *m_connectionsModel;
    QDataWidgetMapper *m_dataWidgetMapper;
};

#endif // LOGINDIALOG_H
