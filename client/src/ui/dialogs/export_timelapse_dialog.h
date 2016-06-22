#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
class ExportTimelapseDialog;
}

class QnFilteredUnitsModel;

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

    void setSpeed(qint64 value);
    qint64 speed() const;

    qint64 frameStepMs() const;

    /* Magic knowledge. We know that the result will be generated with 30 fps. --rvasilenko */
    static const int kResultFps = 30;

private:
    void initControls();

    void setExpectedLengthMs(qint64 value);
    void updateFrameStep(int speed);

    int toSliderScale(int absoluteSpeedValue);
    int fromSliderScale(int sliderValue);

    QString durationMsToString(qint64 durationMs);

    qint64 expectedLengthMeasureUnit() const;
private:
     QScopedPointer<Ui::ExportTimelapseDialog> ui;
     bool m_updating;

     qint64 m_expectedLengthMs;
     qint64 m_sourcePeriodLengthMs;
     qint64 m_frameStepMs;
     int m_maxSpeed;

     QStandardItemModel* m_filteredUnitsModel;
};
