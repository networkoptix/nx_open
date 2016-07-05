#pragma once

#include <utils/email/email_fwd.h>

namespace Ui {
    class SmtpTestConnectionWidget;
}

struct QnTestEmailSettingsReply;

class QnSmtpTestConnectionWidget: public QWidget {
    Q_OBJECT

public:
    QnSmtpTestConnectionWidget(QWidget* parent = nullptr);
    ~QnSmtpTestConnectionWidget();

    bool testSettings(const QnEmailSettings &value);

signals:
    void finished();

private:
    void stopTesting(const QString &result = QString());

    void at_timer_timeout();

private slots:
    void at_testEmailSettingsFinished(int status, const QnTestEmailSettingsReply& reply, int handle);

private:
    QScopedPointer<Ui::SmtpTestConnectionWidget> ui;
    QTimer *m_timeoutTimer;
    int m_testHandle;
};