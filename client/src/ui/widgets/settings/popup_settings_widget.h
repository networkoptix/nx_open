#ifndef POPUP_SETTINGS_WIDGET_H
#define POPUP_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupSettingsWidget;
}

class QnShowBusinessEventsHelper;

class QnPopupSettingsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit QnPopupSettingsWidget(QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    void updateFromSettings();
    void submitToSettings();

private slots:
    void at_showAllCheckBox_toggled(bool checked);
    void at_showBusinessEvents_valueChanged(quint64 value);

private:
    QScopedPointer<Ui::QnPopupSettingsWidget> ui;
    QList<QCheckBox* > m_businessRulesCheckBoxes;
    QList<QCheckBox* > m_systemHealthCheckBoxes;

    QnShowBusinessEventsHelper* m_showBusinessEventsHelper;
};

#endif // POPUP_SETTINGS_WIDGET_H
