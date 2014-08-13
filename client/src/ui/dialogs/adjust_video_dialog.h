#ifndef QN_ADJUST_VIDEO_DIALOG_H
#define QN_ADJUST_VIDEO_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QAbstractButton>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/color_space/image_correction.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnMediaResourceWidget;

namespace Ui {
    class AdjustVideoDialog;
}

// TODO: #Elric rename image enhancement dialog
class QnAdjustVideoDialog : public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnAdjustVideoDialog(QWidget *parent = NULL);
    virtual ~QnAdjustVideoDialog();

    ImageCorrectionParams params() const;

    QnHistogramConsumer *histogramConsumer() const;
    
    void setWidget(QnMediaResourceWidget *widget);

    virtual bool tryClose(bool force) override;
protected:
    virtual void closeEvent(QCloseEvent *e) override;

private:
    void setParams(const ImageCorrectionParams &params);
    void submit();

private slots:
    void at_sliderValueChanged();
    void at_spinboxValueChanged();
    void at_buttonClicked(QAbstractButton* button);
    void at_rendererDestryed();

private:
    QScopedPointer<Ui::AdjustVideoDialog> ui;

    bool m_updateDisabled;
    ImageCorrectionParams m_params;
    QnMediaResourceWidget* m_widget;

    ImageCorrectionParams m_backupParams;
};

#endif // QN_ADJUST_VIDEO_DIALOG_H
