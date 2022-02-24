// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "copy_to_clipboard_table_view.h"

#include <QKeyEvent>

#include <ui/utils/table_export_helper.h>

namespace nx::vms::client::desktop {

void CopyToClipboardTableView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy) && state() != EditingState)
    {
        event->accept();
        copySelectedToClipboard();
        return;
    }

    base_type::keyPressEvent(event);
}

void CopyToClipboardTableView::copySelectedToClipboard()
{
    QnTableExportHelper::copyToClipboard(
        model(), selectionModel()->selectedIndexes(), Qt::ToolTipRole);
}

} // namespace nx::vms::client::desktop
