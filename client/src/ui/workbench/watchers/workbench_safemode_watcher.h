#pragma once

class QnWorkbenchSafeModeWatcher: public QObject {
    Q_OBJECT
public:
    QnWorkbenchSafeModeWatcher(QWidget *parentWidget = nullptr);

    void updateReadOnlyMode(bool readOnly);

    void addControlledWidget(QWidget *widget);
    void addControlledWidgets(QList<QWidget*> widgets);
    void addControlledButton(QDialogButtonBox::StandardButton button);
private:
    QWidget* m_parentWidget;
    QLabel* m_warnLabel;
    QDialogButtonBox* m_buttonBox;
    QList<QWidget*> m_controlledWidgets;
};