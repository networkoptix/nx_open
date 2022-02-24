// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <utils/email/email_fwd.h>

class QTimer;

namespace Ui {
    class SmtpTestConnectionWidget;
}

struct QnTestEmailSettingsReply;

namespace rest {
    template<typename T>
    struct RestResultWithData;
}

class QnSmtpTestConnectionWidget:
    public QWidget,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
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

    QString errorString(const rest::RestResultWithData<QnTestEmailSettingsReply>& result) const;

private slots:
    void at_testEmailSettingsFinished(
        bool success, int handle, const rest::RestResultWithData<QnTestEmailSettingsReply>& reply);

private:
    QScopedPointer<Ui::SmtpTestConnectionWidget> ui;
    QTimer *m_timeoutTimer;
    int m_testHandle;
};
