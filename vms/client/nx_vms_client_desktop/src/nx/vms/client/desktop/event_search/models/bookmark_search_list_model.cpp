// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_list_model.h"

#include <QtGui/QPalette>

#include <client/client_globals.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>

namespace nx::vms::client::desktop {

namespace {

QString iconPath()
{
    return "soft_triggers/user_selectable/bookmark.png";
}

QColor color()
{
    return QPalette().color(QPalette::Light);
}

QPixmap pixmap()
{
    return core::Skin::colorize(qnSkin->pixmap(iconPath()), color());
}

} // namespace

QVariant BookmarkSearchListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case core::DecorationPathRole:
            return "soft_triggers/user_selectable/bookmark.png";

        case Qt::DecorationRole:
            return QVariant::fromValue(pixmap());

        case Qt::ForegroundRole:
            return QVariant::fromValue(color());

        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Bookmarks_Usage;

        default:
            return base_type::data(index, role);
    }
}

} // namespace nx::vms::client::desktop
