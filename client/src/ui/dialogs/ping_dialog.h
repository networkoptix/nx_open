#ifndef PING_DIALOG_H
#define PING_DIALOG_H

#include <QDialog>
#include <utils/ping_utility.h>
#include <ui/dialogs/dialog_base.h>

class QTextEdit;

class QnPingDialog : public QnDialogBase {
    Q_OBJECT
    typedef QnDialogBase base_type;
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
