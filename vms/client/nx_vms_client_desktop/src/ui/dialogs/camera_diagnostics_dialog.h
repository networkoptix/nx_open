// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CAMERA_DIAGNOSTICS_DIALOG_H
#define QN_CAMERA_DIAGNOSTICS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
    class CameraDiagnosticsDialog;
}

namespace CameraDiagnostics {
    class DiagnoseTool;
}

class QnCameraDiagnosticsDialog: public QnSessionAwareButtonBoxDialog {
    Q_OBJECT;
    typedef QnSessionAwareButtonBoxDialog base_type;
public:
    QnCameraDiagnosticsDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = {});
    virtual ~QnCameraDiagnosticsDialog();

    void setResource(const QnVirtualCameraResourcePtr &resource);
    QnVirtualCameraResourcePtr resource();

    bool isStarted() { return m_started; }
    bool isFinished() { return m_finished; }

    void restart();
    void stop();

private:
    QString diagnosticsStepText(int stepType);

private slots:
    void retranslateUi();
    void updateOkButtonEnabled();
    void clearLog();

    void at_tool_diagnosticsStepStarted(int stepType);
    void at_tool_diagnosticsStepResult(int stepType, bool result, const QString &errorMessage);
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
