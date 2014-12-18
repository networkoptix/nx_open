#ifndef MERGE_SYSTEMS_DIALOG_H
#define MERGE_SYSTEMS_DIALOG_H

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

namespace Ui {
    class QnMergeSystemsDialog;
}

class QnMergeSystemsTool;
struct QnModuleInformation;

class QnMergeSystemsDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT

public:
    explicit QnMergeSystemsDialog(QWidget *parent = 0);
    ~QnMergeSystemsDialog();

    QUrl url() const;
    QString password() const;

private slots:
    void at_urlComboBox_activated(int index);
    void at_urlComboBox_editingFinished();
    void at_testConnectionButton_clicked();
    void at_mergeButton_clicked();

    void at_mergeTool_systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode);
    void at_mergeTool_mergeFinished(int errorCode, const QnModuleInformation &moduleInformation);

private:
    void updateKnownSystems();
    void updateErrorLabel(const QString &error);
    void updateConfigurationBlock();

private:
    QScopedPointer<Ui::QnMergeSystemsDialog> ui;
    QPushButton *m_mergeButton;

    QnMergeSystemsTool *m_mergeTool;

    QnMediaServerResourcePtr m_discoverer;
    QUrl m_url;
    QString m_user;
    QString m_password;
};

#endif // MERGE_SYSTEMS_DIALOG_H
