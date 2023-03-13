// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>
#include <QtWidgets/QAbstractButton>

namespace nx::vms::client::desktop {

class ScreenRecordingIndicator: public QAbstractButton
{
    Q_OBJECT
    using base_type = QAbstractButton;

public:
    ScreenRecordingIndicator(QWidget* parent = nullptr);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
};

} // namespace nx::vms::client::desktop
