// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/pending_operation.h>
#include <nx/vms/api/data/watermark_settings.h>
#include <ui/dialogs/common/button_box_dialog.h>

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
    static bool editSettings(api::WatermarkSettings& settings, QWidget* parent);

private:
    void loadDataToUi();
    void updateDataFromControls();

    void drawPreview();

private:
    Q_DISABLE_COPY(WatermarkPreviewDialog)

    QScopedPointer<Ui::WatermarkPreviewDialog> ui;

    api::WatermarkSettings m_settings;
    QPixmap m_baseImage;
    bool m_lockUpdate = false;
    nx::utils::PendingOperation m_repaintOperation;
};

} // namespace nx::vms::client::desktop
