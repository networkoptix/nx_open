#ifndef POPUP_SETTINGS_WIDGET_H
#define POPUP_SETTINGS_WIDGET_H

#include <QWidget>
#include <QtGui/QCheckBox>

namespace Ui {
    class QnPopupSettingsWidget;
}

class QnClientSettings;

class QnPopupSettingsWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnPopupSettingsWidget(QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    void updateFromSettings(QnClientSettings* settings);
    void submitToSettings(QnClientSettings* settings);

private slots:
    void at_showAllCheckBox_toggled(bool checked);

private:
    QScopedPointer<Ui::QnPopupSettingsWidget> ui;
    QList<QCheckBox* > m_businessRulesCheckBoxes;
    QList<QCheckBox* > m_systemHealthCheckBoxes;
};

#endif // POPUP_SETTINGS_WIDGET_H
