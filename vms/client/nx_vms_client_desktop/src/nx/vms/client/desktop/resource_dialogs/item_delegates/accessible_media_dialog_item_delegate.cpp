// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_media_dialog_item_delegate.h"

#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

namespace {

static const int kIndirectAccessColumnWidth = nx::style::Metrics::kViewRowHeight;

} // namespace

namespace nx::vms::client::desktop {

AccessibleMediaDialogItemDelegate::AccessibleMediaDialogItemDelegate(
    nx::core::access::ResourceAccessProvider* accessProvider,
    QObject* parent)
    :
    base_type(parent),
    m_resourceAccessProvider(accessProvider)
{
}

void AccessibleMediaDialogItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    base_type::paint(painter, styleOption, index);

    if (isSeparator(index) || !m_subject.isValid() || !m_resourceAccessProvider)
        return;

    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!resource)
        return;

    QIcon indirectAccessIcon;
    const auto accessLevels = m_resourceAccessProvider->accessLevels(m_subject, resource);
    if (accessLevels.contains(nx::core::access::Source::layout))
        indirectAccessIcon = qnResIconCache->icon(QnResourceIconCache::SharedLayout);

    if (accessLevels.contains(nx::core::access::Source::videowall))
        indirectAccessIcon = qnResIconCache->icon(QnResourceIconCache::VideoWall);

    if (indirectAccessIcon.isNull())
        return;

    const auto iconState = styleOption.state.testFlag(QStyle::State_Enabled)
        ? QIcon::Normal
        : QIcon::Disabled;

    const auto pixmap = indirectAccessIcon.pixmap(nx::style::Metrics::kDefaultIconSize, iconState);
    auto itemRect = styleOption.rect;
    itemRect.setRight(itemRect.right() - decoratorWidth(index));
    itemRect.setLeft(itemRect.right() - kIndirectAccessColumnWidth);

    const auto iconSize = pixmap.size() / pixmap.devicePixelRatio();
    const auto iconTopLeft = itemRect.topLeft()
        + QPoint(itemRect.width() - iconSize.width(), (itemRect.height() - iconSize.height()) / 2);

    painter->drawPixmap(iconTopLeft, pixmap);
}

int AccessibleMediaDialogItemDelegate::decoratorWidth(const QModelIndex& index) const
{
    return base_type::decoratorWidth(index) + kIndirectAccessColumnWidth;
}

void AccessibleMediaDialogItemDelegate::setSubject(const QnResourceAccessSubject& subject)
{
    m_subject = subject;
}

} // namespace nx::vms::client::desktop
