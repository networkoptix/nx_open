#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
class ExportTimelapseDialog;
}

class QnExportTimelapseDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    explicit QnExportTimelapseDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    virtual ~QnExportTimelapseDialog();

    static qint64 kMinimalSourcePeriodLength;
    qint64 sourcePeriodLengthMs() const;
    void setSourcePeriodLengthMs(qint64 lengthMs);

    qint64 frameStepMs() const;

private:
    void initControls();

    void setExpectedLength(qint64 value);

private:
     QScopedPointer<Ui::ExportTimelapseDialog> ui;
     bool m_updating;

     qint64 m_expectedLengthMs;
     qint64 m_sourcePeriodLengthMs;
     qint64 m_frameStepMs;
};
