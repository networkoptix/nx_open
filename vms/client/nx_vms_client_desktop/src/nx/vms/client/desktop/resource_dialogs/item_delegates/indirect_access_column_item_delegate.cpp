// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "indirect_access_column_item_delegate.h"

#include <utils/common/scoped_painter_rollback.h>

namespace {

static constexpr auto kIndirectAccessIconOpacity = 0.5;

} // namespace

namespace nx::vms::client::desktop {

IndirectAccessColumnItemDelegate::IndirectAccessColumnItemDelegate(QObject* parent):
    base_type(parent)
{
}

void IndirectAccessColumnItemDelegate::paintContents(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QnScopedPainterOpacityRollback opacityRollback(painter, kIndirectAccessIconOpacity);
    base_type::paintContents(painter, styleOption, index);
}

} // namespace nx::vms::client::desktop
