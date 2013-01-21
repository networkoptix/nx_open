#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QWidget>

namespace Ui {
    class QnSmtpSettingsWidget;
}

class QnSmtpSettingsWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();
    //TODO: #GDM read-write settings
private slots:
    void at_portComboBox_currentIndexChanged(int index);

private:
    Ui::QnSmtpSettingsWidget *ui;
};

#endif // SMTP_SETTINGS_WIDGET_H
