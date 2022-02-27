// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "table_view.h"

namespace nx::vms::client::desktop {

class CopyToClipboardTableView: public TableView
{
    Q_OBJECT
    using base_type = TableView;

public:
    CopyToClipboardTableView(QWidget* parent = nullptr): base_type(parent) {}

    void copySelectedToClipboard();

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
};

} // namespace nx::vms::client::desktop
