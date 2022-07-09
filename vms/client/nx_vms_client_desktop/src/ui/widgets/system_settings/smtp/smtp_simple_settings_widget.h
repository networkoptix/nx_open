// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <utils/email/email_fwd.h>

namespace Ui
{
    class SmtpSimpleSettingsWidget;
}

struct QnSimpleSmtpSettings
{
    QString email;
    QString password;
    QString signature;
    QString supportEmail;

    /** Convert to full settings, using provided base value if not valid. */
    QnEmailSettings toSettings(const QnEmailSettings &base) const;
    static QnSimpleSmtpSettings fromSettings(const QnEmailSettings &source);
};

class QnSmtpSimpleSettingsWidget: public QWidget
{
    Q_OBJECT

public:
    QnSmtpSimpleSettingsWidget(QWidget* parent = nullptr);
    ~QnSmtpSimpleSettingsWidget();

    QnSimpleSmtpSettings settings() const;
    void setSettings(const QnSimpleSmtpSettings &value);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setHasRemotePassword(bool value);

signals:
    void settingsChanged();

private:
    QScopedPointer<Ui::SmtpSimpleSettingsWidget> ui;
    bool m_updating;
    bool m_readOnly;
};
