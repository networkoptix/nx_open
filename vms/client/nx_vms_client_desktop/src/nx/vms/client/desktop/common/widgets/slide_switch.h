// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>

namespace nx::vms::client::desktop {

/**
 * Standalone checkbox styled as a slide switch. Has fixed size, doesn't display text stored in the
 * <tt>text</tt> property. Note: for button-line slide switch see <tt>QPushButton</tt> with
 * <tt>checkable</tt> property set to true.
 */
class SlideSwitch: public QCheckBox
{
    Q_OBJECT
    using base_type = QCheckBox;

public:
    SlideSwitch(QWidget* parent = nullptr);

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
};

} // namespace nx::vms::client::desktop
