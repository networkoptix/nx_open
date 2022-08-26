// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <utils/email/email.h>

namespace Ui { class SmtpSimpleSettingsWidget; }

class QnSmtpSimpleSettingsWidget: public QWidget
{
    Q_OBJECT

public:
    QnSmtpSimpleSettingsWidget(QWidget* parent = nullptr);
    ~QnSmtpSimpleSettingsWidget();

    QnEmailSettings settings(const QnEmailSettings& baseSettings = QnEmailSettings()) const;
    void setSettings(const QnEmailSettings& value);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    bool hasRemotePassword() const;
    void setHasRemotePassword(bool value);

signals:
    void settingsChanged();

private:
    QScopedPointer<Ui::SmtpSimpleSettingsWidget> ui;
    bool m_updating = false;
    bool m_readOnly = false;
};
