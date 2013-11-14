#ifndef GENERAL_PREFERENCES_WIDGET_H
#define GENERAL_PREFERENCES_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnGeneralPreferencesWidget;
}

class QnGeneralPreferencesWidget : public QWidget, protected QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit QnGeneralPreferencesWidget(QWidget *parent = 0);
    ~QnGeneralPreferencesWidget();

    void submitToSettings();
    void updateFromSettings();

    bool confirmCriticalSettings();
private:
    void initTranslations();

private slots:
    void at_browseMainMediaFolderButton_clicked();
    void at_addExtraMediaFolderButton_clicked();
    void at_removeExtraMediaFolderButton_clicked();
    void at_extraMediaFoldersList_selectionChanged();
    void at_timeModeComboBox_activated();
    void at_pluginManager_pluginLoaded();
    void at_downmixAudioCheckBox_toggled(bool checked);
    void at_languageComboBox_currentIndexChanged(int index);
    void at_browseLogsButton_clicked();


private:
    QScopedPointer<Ui::QnGeneralPreferencesWidget> ui;

    bool m_oldDownmix;
    int m_oldLanguage;
    bool m_oldHardwareAcceleration;
};

#endif // GENERAL_PREFERENCES_WIDGET_H
