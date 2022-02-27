// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    explicit QnPingDialog(QWidget* parent = nullptr, Qt::WindowFlags flags = {});

    void startPings();

    void setHostAddress(const QString &hostAddress);

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void at_pingUtility_pingResponse(const QnPingUtility::PingResponse &response);

private:
    QString responseToString(const QnPingUtility::PingResponse &response) const;

private:
    QTextEdit *m_pingText;
    QnPingUtility *m_pingUtility;
};

#endif // PING_DIALOG_H
