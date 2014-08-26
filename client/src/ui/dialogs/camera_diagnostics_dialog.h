#ifndef QN_CAMERA_DIAGNOSTICS_DIALOG_H
#define QN_CAMERA_DIAGNOSTICS_DIALOG_H

#include <QDialog>
#include <QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class CameraDiagnosticsDialog;
}

namespace CameraDiagnostics {
    class DiagnoseTool;
}

class QnCameraDiagnosticsDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT;
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    QnCameraDiagnosticsDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnCameraDiagnosticsDialog();

    void setResource(const QnVirtualCameraResourcePtr &resource);
    QnVirtualCameraResourcePtr resource();

    bool isStarted() { return m_started; }
    bool isFinished() { return m_finished; }

    void restart();
    void stop();

private:
    static QString diagnosticsStepText(int stepType);

private slots:
    void updateTitleText();
    void updateOkButtonEnabled();
    void clearLog();

    void at_tool_diagnosticsStepStarted(CameraDiagnostics::Step::Value stepType);
    void at_tool_diagnosticsStepResult(CameraDiagnostics::Step::Value stepType, bool result, const QString &errorMessage);
    void at_tool_diagnosticsDone();

    void at_copyButton_clicked();

private:
    QScopedPointer<Ui::CameraDiagnosticsDialog> ui;
    
    QnVirtualCameraResourcePtr m_resource;
    CameraDiagnostics::DiagnoseTool *m_tool;
    bool m_started, m_finished;

    QString m_lastLine;
};


#endif // QN_CAMERA_DIAGNOSTICS_DIALOG_H
