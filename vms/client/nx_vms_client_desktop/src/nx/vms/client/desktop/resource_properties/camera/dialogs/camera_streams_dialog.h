// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

#include "../data/camera_stream_urls.h"

namespace Ui { class CameraStreamsDialog; }

namespace nx::vms::client::desktop {

class CameraStreamsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit CameraStreamsDialog(QWidget* parent);
    virtual ~CameraStreamsDialog() override;

    CameraStreamUrls streams() const;
    void setStreams(const CameraStreamUrls& value);

private:
    const QScopedPointer<Ui::CameraStreamsDialog> ui;
};

} // namespace nx::vms::client::desktop
