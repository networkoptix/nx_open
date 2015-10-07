#pragma once

class QnWorkbenchSafeModeWatcher: public QObject {
    Q_OBJECT
public:
    enum class ControlMode {
        Hide,
        Disable,
        MakeReadOnly

    };

    QnWorkbenchSafeModeWatcher(QWidget *parentWidget = nullptr);

    void updateReadOnlyMode();

    void addWarningLabel(QDialogButtonBox *buttonBox);
    void addControlledWidget(QWidget *widget, ControlMode mode);
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