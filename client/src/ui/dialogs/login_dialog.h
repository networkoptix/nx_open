#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtGui/QDialog>
#include "plugins/resources/archive/avi_files/avi_resource.h"

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveReader;
class CLVideoCamera;
class QnResourceWidgetRenderer;
class QnRenderingWidget;

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
    virtual void reject() override;
    void reset();

protected:
    virtual void changeEvent(QEvent *event) override;

    QUrl currentUrl();

    void updateStoredConnections();

private slots:
    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_configureConnectionsButton_clicked();
    void at_testButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(int index);
    void at_testFinished(int status, const QByteArray &data, const QByteArray &errorString, int requestHandle);

private:
    Q_DISABLE_COPY(LoginDialog)

    QScopedPointer<Ui::LoginDialog> ui;
    QWeakPointer<QnWorkbenchContext> m_context;
    QStandardItemModel *m_connectionsModel;
    QDataWidgetMapper *m_dataWidgetMapper;
    int m_requestHandle;

    QnRenderingWidget* m_renderingWidget;
};

#endif // LOGINDIALOG_H
