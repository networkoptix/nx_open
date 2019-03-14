#pragma once

#include <utils/email/email_fwd.h>

namespace Ui {
    class SmtpAdvancedSettingsWidget;
}

class QnSmtpAdvancedSettingsWidget: public QWidget {
    Q_OBJECT

public:
    QnSmtpAdvancedSettingsWidget(QWidget* parent = nullptr);
    ~QnSmtpAdvancedSettingsWidget();

    QnEmailSettings settings() const;
    void setSettings(const QnEmailSettings &value);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    void settingsChanged();

private:
    void setConnectionType(QnEmail::ConnectionType connectionType, int port = 0);

    void at_portComboBox_currentIndexChanged(int index);

private:
    QScopedPointer<Ui::SmtpAdvancedSettingsWidget> ui;
    bool m_updating;
    bool m_readOnly;
};