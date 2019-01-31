#pragma once

#include <client_core/connection_context_aware.h>

class QLabel;

class QnWorkbenchSafeModeWatcher: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
public:
    enum class ControlMode {
        Hide,
        Disable,
        MakeReadOnly

    };

    QnWorkbenchSafeModeWatcher(QWidget *parentWidget = nullptr);

    /**
     * @brief addWarningLabel                   Place warning label with text 'System in Safe Mode' directly on the button box.
     * @param buttonBox                         Widget to place label.
     * @param beforeWidget                      Label will be placed to the left of the given widget. Default value is OK button.
     */
    void addWarningLabel(QDialogButtonBox *buttonBox, QWidget *beforeWidget = nullptr);


    void addControlledWidget(QWidget *widget, ControlMode mode);

private:
    void updateReadOnlyMode();

private:
    struct ControlledWidget {
        QWidget* widget;
        ControlMode mode;
        ControlledWidget(QWidget *widget, ControlMode mode):
            widget(widget), mode(mode){}
    };

    QWidget* m_parentWidget;
    QLabel* m_warnLabel;
    QList<ControlledWidget> m_controlledWidgets;
};