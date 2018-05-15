#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

#include "../models/camera_streams_model.h"

namespace Ui { class CameraStreamsDialog; }

namespace nx {
namespace client {
namespace desktop {

class CameraStreamsDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit CameraStreamsDialog(QWidget* parent = nullptr);
    virtual ~CameraStreamsDialog() override;

    CameraStreamsModel model() const;
    void setModel(const CameraStreamsModel& model);

private:
    const QScopedPointer<Ui::CameraStreamsDialog> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
