// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QLabel>

namespace nx::vms::client::desktop {

/**
 * Key hint label is a plain label the contents of which is inscribed into styled rectangle.
 * It's used for display components of keyboard shortcuts, however it doesn't hold any logic
 * related to the string representation of the keys etc. It's just a visual primitive.
 */
class KeyHintLabel: public QLabel
{
    Q_OBJECT
    using base_type = QLabel;

public:
    KeyHintLabel(QWidget* parent = nullptr);
    KeyHintLabel(const QString& text, QWidget* parent = nullptr);

    virtual QSize minimumSizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;
};

} // namespace nx::vms::client::desktop
