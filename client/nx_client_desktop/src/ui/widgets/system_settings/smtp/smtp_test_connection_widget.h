#pragma once

#include <client_core/connection_context_aware.h>

#include <utils/email/email_fwd.h>

namespace Ui {
    class SmtpTestConnectionWidget;
}

struct QnTestEmailSettingsReply;

class QnSmtpTestConnectionWidget: public QWidget, public QnConnectionContextAware
{
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

    QString errorString(const QnTestEmailSettingsReply& result) const;

private slots:
    void at_testEmailSettingsFinished(int status, const QnTestEmailSettingsReply& reply, int handle);

private:
    QScopedPointer<Ui::SmtpTestConnectionWidget> ui;
    QTimer *m_timeoutTimer;
    int m_testHandle;
};