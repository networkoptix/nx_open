#ifndef QN_ADJUST_VIDEO_DIALOG_H
#define QN_ADJUST_VIDEO_DIALOG_H

#include <QtGui/QDialog>

#include <client/client_settings.h>
#include <utils/network/networkoptixmodulefinder.h>
#include <ui/workbench/workbench_context_aware.h>
#include "utils/color_space/image_correction.h"

class QnResourceWidgetRenderer;
class QnMediaResourceWidget;

namespace Ui {
    class AdjustVideoDialog;
}

class QnAdjustVideoDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QDialog base_type;
public:
    explicit QnAdjustVideoDialog(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);
    virtual ~QnAdjustVideoDialog();

    ImageCorrectionParams params() const;

    QnHistogramConsumer * getHystogramConsumer() const;
    void setWidget(QnMediaResourceWidget* widget);
private:
    void setParams(const ImageCorrectionParams& params);
private slots:
    void at_sliderValueChanged();
    void at_spinboxValueChanged();
    void at_buttonClicked(QAbstractButton* button);
    void at_rendererDestryed();
private:
    void uiToParams();
private:
    QScopedPointer<Ui::AdjustVideoDialog> ui;
    bool m_updateDisabled;
    ImageCorrectionParams m_params;
    QnMediaResourceWidget* m_widget;

    typedef QMap<QnMediaResourceWidget*, ImageCorrectionParams> ParamsMap;
    ParamsMap m_backupParams;
};

#endif // QN_ADJUST_VIDEO_DIALOG_H
