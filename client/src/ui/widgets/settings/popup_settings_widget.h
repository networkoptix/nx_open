#ifndef POPUP_SETTINGS_WIDGET_H
#define POPUP_SETTINGS_WIDGET_H

#include <QWidget>
#include <QtGui/QCheckBox>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupSettingsWidget;
}

class QnKvPairUsageHelper;

class QnPopupSettingsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    
public:
    explicit QnPopupSettingsWidget(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    void submit();

private slots:
    void at_showAllCheckBox_toggled(bool checked);
    void at_showBusinessEvents_valueChanged(const QString &value);
    void at_showSystemHealth_valueChanged(const QString &value);
    void at_context_userChanged();

private:
    QScopedPointer<Ui::QnPopupSettingsWidget> ui;
    QList<QCheckBox* > m_businessRulesCheckBoxes;
    QList<QCheckBox* > m_systemHealthCheckBoxes;

    QnKvPairUsageHelper* m_showBusinessEventsHelper;
    QnKvPairUsageHelper* m_showSystemHealthHelper;
};

#endif // POPUP_SETTINGS_WIDGET_H
