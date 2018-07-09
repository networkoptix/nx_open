#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <utils/common/watermark_settings.h>

class QPixmap;
struct QnWatermarkSettings;
class QnWatermarkPainter;

namespace Ui {
    class QnWatermarkPreviewDialog;
}

class QnWatermarkPreviewDialog : public QnButtonBoxDialog
{
    Q_OBJECT
public:

    QnWatermarkPreviewDialog(QWidget* parent);
    ~QnWatermarkPreviewDialog();

    /** returns true if settings were changed */
    static bool editSettings(QnWatermarkSettings& settings, QWidget* parent);

private:
    void loadDataToUi();
    void updateDataFromControls();

    void drawPreview();

private:
    Q_DISABLE_COPY(QnWatermarkPreviewDialog)

    QScopedPointer<Ui::QnWatermarkPreviewDialog> ui;

    QnWatermarkSettings m_settings;
    QScopedPointer<QnWatermarkPainter> m_painter;
    QScopedPointer<QPixmap> m_baseImage;
};
