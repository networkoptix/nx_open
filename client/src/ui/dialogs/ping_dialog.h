#ifndef PING_DIALOG_H
#define PING_DIALOG_H

#include <QtWidgets/QDialog>
#include <utils/ping_utility.h>
#include <ui/dialogs/common/dialog.h>

class QTextEdit;

class QnPingDialog : public QnDialog {
    Q_OBJECT
    typedef QnDialog base_type;
public:
    explicit QnPingDialog(QWidget *parent = 0, Qt::WindowFlags flags = 0);

    void startPings();

    void setHostAddress(const QString &hostAddress);

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void at_pingUtility_pingResponce(const QnPingUtility::PingResponce &responce);

private:
    QString responceToString(const QnPingUtility::PingResponce &responce) const;

private:
    QTextEdit *m_pingText;
    QnPingUtility *m_pingUtility;
};

#endif // PING_DIALOG_H
