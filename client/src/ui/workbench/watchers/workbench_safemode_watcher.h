#pragma once

class QnWorkbenchSafeModeWatcher: public QObject {
    Q_OBJECT
public:
    QnWorkbenchSafeModeWatcher(QWidget *parentWidget = nullptr);

    void updateReadOnlyMode();

    void addWarningLabel(QDialogButtonBox *buttonBox);

    void addAutoHiddenWidget(QWidget *widget);
    void addAutoReadOnlyWidget(QWidget *widget);
private:
    QWidget* m_parentWidget;
    QLabel* m_warnLabel;
    QList<QWidget*> m_autoHiddenWidgets;
    QList<QWidget*> m_autoReadOnlyWidgets;
};