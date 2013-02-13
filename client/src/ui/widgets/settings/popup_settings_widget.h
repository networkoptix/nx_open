#ifndef POPUP_SETTINGS_WIDGET_H
#define POPUP_SETTINGS_WIDGET_H

#include <QWidget>
#include <QtGui/QCheckBox>

namespace Ui {
    class QnPopupSettingsWidget;
}

class QnSettings;

class QnPopupSettingsWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnPopupSettingsWidget(QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    void updateFromSettings(QnSettings* settings);
    void submitToSettings(QnSettings* settings);

private slots:
    void at_showAllCheckBox_toggled(bool checked);

private:
    QScopedPointer<Ui::QnPopupSettingsWidget> ui;
    QList<QCheckBox* > m_checkBoxes;
};

#endif // POPUP_SETTINGS_WIDGET_H
