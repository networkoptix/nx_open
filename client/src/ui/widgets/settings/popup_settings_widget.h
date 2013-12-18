#ifndef POPUP_SETTINGS_WIDGET_H
#define POPUP_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupSettingsWidget;
}

class QnBusinessEventsFilterKvPairAdapter;

class QnPopupSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnPopupSettingsWidget(QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    virtual void submitToSettings() override;
    virtual void updateFromSettings() override;

private slots:
    void at_showAllCheckBox_toggled(bool checked);
    void at_showBusinessEvents_valueChanged(quint64 value);

private:
    QScopedPointer<Ui::QnPopupSettingsWidget> ui;
    QList<QCheckBox* > m_businessRulesCheckBoxes;
    QList<QCheckBox* > m_systemHealthCheckBoxes;
    QScopedPointer<QnBusinessEventsFilterKvPairAdapter> m_adapter;
};

#endif // POPUP_SETTINGS_WIDGET_H
