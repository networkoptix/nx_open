// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>

namespace nx::vms::client::desktop {
namespace menu {

class MenuEventFilter: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit MenuEventFilter(QObject* parent = nullptr);

    virtual bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace menu
} // namespace nx::vms::client::desktop
