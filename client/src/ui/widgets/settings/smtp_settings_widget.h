#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QWidget>

#include <api/model/kvpair.h>

namespace Ui {
    class QnSmtpSettingsWidget;
}

class QnSmtpSettingsWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    void update();
    void submit();
private slots:
    void at_portComboBox_currentIndexChanged(int index);

    void at_settings_received(int status, const QByteArray& errorString, const QnKvPairList& settings, int handle);

private:
    QScopedPointer<Ui::QnSmtpSettingsWidget> ui;

    int m_requestHandle;
};

#endif // SMTP_SETTINGS_WIDGET_H
