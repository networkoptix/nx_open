#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <utils/common/watermark_settings.h>

class QPixmap;

namespace nx {
namespace client {
namespace desktop {

class WatermarkPainter;

namespace Ui { class WatermarkPreviewDialog;}

class WatermarkPreviewDialog: public QnButtonBoxDialog
{
    Q_OBJECT

public:
    WatermarkPreviewDialog(QWidget* parent);
    ~WatermarkPreviewDialog();

    /** returns true if settings were changed */
    static bool editSettings(QnWatermarkSettings& settings, QWidget* parent);

private:
    void loadDataToUi();
    void updateDataFromControls();

    void drawPreview();

private:
    Q_DISABLE_COPY(WatermarkPreviewDialog)

    QScopedPointer<Ui::WatermarkPreviewDialog> ui;

    QnWatermarkSettings m_settings;
    QScopedPointer<WatermarkPainter> m_painter;
    QScopedPointer<QPixmap> m_baseImage;
    bool m_lockUpdate = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
