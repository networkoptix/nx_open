// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QAbstractButton>

class QTabBar;

namespace nx::vms::client::desktop {

class CloseTabButton: public QAbstractButton
{
    Q_OBJECT
    using base_type = QAbstractButton;

public:
    static constexpr int kFixedIconWidth = 16;
    static constexpr int kFixedIconHeight = 16;

    explicit CloseTabButton(QWidget* parent = 0);
    virtual void paintEvent(QPaintEvent* event) override;

    static CloseTabButton* createForTab(QTabBar* tabBar, int index);
};

} // namespace nx::vms::client::desktop
