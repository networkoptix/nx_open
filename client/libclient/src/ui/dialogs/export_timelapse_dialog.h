#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class ExportTimelapseDialog;
}

class QnFilteredUnitsModel;
class QStandardItemModel;

class QnExportTimelapseDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;
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

    void setExpectedLengthMsInternal(qint64 value);
    void updateFrameStepInternal(int speed);
    void updateExpectedLengthRangeInternal(int index);

    int toSliderScale(int absoluteSpeedValue) const;
    int fromSliderScale(int sliderValue) const;

    QString durationMsToString(qint64 durationMs);

    qint64 expectedLengthMeasureUnit(int index = -1) const;
private:
     QScopedPointer<Ui::ExportTimelapseDialog> ui;
     bool m_updating;

     qint64 m_speed;
     qint64 m_sourcePeriodLengthMs;
     qint64 m_frameStepMs;
     int m_maxSpeed;

     QStandardItemModel* m_filteredUnitsModel;
};
