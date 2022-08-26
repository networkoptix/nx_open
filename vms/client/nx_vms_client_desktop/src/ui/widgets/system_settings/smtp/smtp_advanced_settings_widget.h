// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <utils/email/email_fwd.h>

namespace Ui { class SmtpAdvancedSettingsWidget; }

class QnSmtpAdvancedSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    QnSmtpAdvancedSettingsWidget(QWidget* parent = nullptr);
    ~QnSmtpAdvancedSettingsWidget();

    QnEmailSettings settings() const;
    void setSettings(const QnEmailSettings& value);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    bool hasRemotePassword() const;
    void setHasRemotePassword(bool value);

signals:
    void settingsChanged();

private:
    void setConnectionType(QnEmail::ConnectionType connectionType, int port = 0);
    void at_portComboBox_currentIndexChanged(int index);

private:
    QScopedPointer<Ui::SmtpAdvancedSettingsWidget> ui;
    bool m_updating = false;
    bool m_readOnly = false;
};
