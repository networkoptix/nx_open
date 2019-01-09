#ifndef QN_ADJUST_VIDEO_DIALOG_H
#define QN_ADJUST_VIDEO_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QAbstractButton>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/color_space/image_correction.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QnMediaResourceWidget;

namespace Ui {
    class AdjustVideoDialog;
}

// TODO: #Elric rename image enhancement dialog
class QnAdjustVideoDialog : public QnSessionAwareButtonBoxDialog {
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnAdjustVideoDialog(QWidget *parent);
    virtual ~QnAdjustVideoDialog();

    nx::vms::api::ImageCorrectionData params() const;

    QnHistogramConsumer *histogramConsumer() const;

    void setWidget(QnMediaResourceWidget *widget);

    virtual bool tryClose(bool force) override;
protected:
    virtual void closeEvent(QCloseEvent *e) override;

private:
    void setParams(const nx::vms::api::ImageCorrectionData &params);
    void submit();

private slots:
    void at_sliderValueChanged();
    void at_spinboxValueChanged();
    void at_buttonClicked(QAbstractButton* button);
    void at_rendererDestryed();

private:
    QScopedPointer<Ui::AdjustVideoDialog> ui;

    bool m_updateDisabled;
    nx::vms::api::ImageCorrectionData m_params;
    QPointer<QnMediaResourceWidget> m_widget;

    nx::vms::api::ImageCorrectionData m_backupParams;
};

#endif // QN_ADJUST_VIDEO_DIALOG_H
