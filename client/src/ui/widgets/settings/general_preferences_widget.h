#ifndef GENERAL_PREFERENCES_WIDGET_H
#define GENERAL_PREFERENCES_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class GeneralPreferencesWidget;
}

class QnGeneralPreferencesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnGeneralPreferencesWidget(QWidget *parent = 0);
    ~QnGeneralPreferencesWidget();

    virtual void submitToSettings() override;
    virtual void updateFromSettings() override;

    virtual bool confirm() override;
private:
    void initTranslations();
    void updateCurrentBgColor();

private slots:
    void at_browseMainMediaFolderButton_clicked();
    void at_addExtraMediaFolderButton_clicked();
    void at_removeExtraMediaFolderButton_clicked();
    void at_extraMediaFoldersList_selectionChanged();
    void at_timeModeComboBox_activated();

    void at_browseLogsButton_clicked();

private:
    QScopedPointer<Ui::GeneralPreferencesWidget> ui;

    bool m_oldDownmix;
    bool m_oldDoubleBuffering;
    int m_oldLanguage;
    int m_oldSkin;
    QColor m_bgColor;
};

#endif // GENERAL_PREFERENCES_WIDGET_H
