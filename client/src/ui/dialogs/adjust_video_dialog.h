#ifndef QN_ADJUST_VIDEO_DIALOG_H
#define QN_ADJUST_VIDEO_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QAbstractButton>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/color_space/image_correction.h>

class QnMediaResourceWidget;
class QnWorkbenchStateDelegate;

namespace Ui {
    class AdjustVideoDialog;
}

// TODO: #Elric rename image enhancement dialog
class QnAdjustVideoDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QDialog base_type;

public:
    explicit QnAdjustVideoDialog(QWidget *parent = NULL);
    virtual ~QnAdjustVideoDialog();

    ImageCorrectionParams params() const;

    QnHistogramConsumer *histogramConsumer() const;
    
    void setWidget(QnMediaResourceWidget *widget);

    bool tryClose(bool force);
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
    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;

    bool m_updateDisabled;
    ImageCorrectionParams m_params;
    QnMediaResourceWidget* m_widget;

    ImageCorrectionParams m_backupParams;
};

#endif // QN_ADJUST_VIDEO_DIALOG_H
