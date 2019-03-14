#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <utils/common/watermark_settings.h>

class QPixmap;

namespace nx::vms::client::desktop {

namespace Ui { class WatermarkPreviewDialog;}

class WatermarkPreviewDialog: public QnButtonBoxDialog
{
    Q_OBJECT

public:
    WatermarkPreviewDialog(QWidget* parent);
    ~WatermarkPreviewDialog();

    /** Returns true if settings were changed. */
    static bool editSettings(QnWatermarkSettings& settings, QWidget* parent);

private:
    void loadDataToUi();
    void updateDataFromControls();

    void drawPreview();

private:
    Q_DISABLE_COPY(WatermarkPreviewDialog)

    QScopedPointer<Ui::WatermarkPreviewDialog> ui;

    QnWatermarkSettings m_settings;
    QScopedPointer<QPixmap> m_baseImage;
    bool m_lockUpdate = false;
};

} // namespace nx::vms::client::desktop
