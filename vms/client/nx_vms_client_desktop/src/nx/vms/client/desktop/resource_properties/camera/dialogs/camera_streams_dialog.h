#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

#include "../data/camera_stream_urls.h"

namespace Ui { class CameraStreamsDialog; }

namespace nx::vms::client::desktop {

class CameraStreamsDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit CameraStreamsDialog(QWidget* parent = nullptr);
    virtual ~CameraStreamsDialog() override;

    CameraStreamUrls streams() const;
    void setStreams(const CameraStreamUrls& value);

private:
    const QScopedPointer<Ui::CameraStreamsDialog> ui;
};

} // namespace nx::vms::client::desktop
