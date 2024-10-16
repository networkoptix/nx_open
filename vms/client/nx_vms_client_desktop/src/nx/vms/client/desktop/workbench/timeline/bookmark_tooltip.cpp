// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_tooltip.h"

#include <QtQuick/QQuickItem>

#include <core/resource/camera_bookmark.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

QVariantList toVariantList(const common::CameraBookmarkList& bookmarks)
{
    QVariantList result;
    for (const auto& bookmark: bookmarks)
    {
        QVariantMap bookmarkData;
        bookmarkData.insert("name", bookmark.name);
        bookmarkData.insert("description", bookmark.description);
        bookmarkData.insert("tags", QVariant::fromValue(bookmark.tags));
        bookmarkData.insert(
            "dateTime", QDateTime::fromMSecsSinceEpoch(bookmark.creationTime().count()));

        result.push_back(bookmarkData);
    }

    return result;
}

} // namespace

BookmarkTooltip::BookmarkTooltip(common::CameraBookmarkList bookmarks, QWidget* parent):
    QQuickWidget(appContext()->qmlEngine(), parent),
    m_bookmarks{std::move(bookmarks)}
{
    // For semi-transparency:
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);

    setResizeMode(QQuickWidget::SizeViewToRootObject);
    setSource(QUrl("Nx/Timeline/BookmarkTooltip.qml"));

    setClearColor(Qt::transparent);

    const auto flags = windowFlags();
    setWindowFlags(flags | Qt::ToolTip | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);

    rootObject()->setProperty("self", QVariant::fromValue(this));
    rootObject()->setProperty("bookmarks", toVariantList(m_bookmarks));
}

void BookmarkTooltip::setAllowExport(bool allowExport)
{
    rootObject()->setProperty("allowExport", allowExport);
}

void BookmarkTooltip::setReadOnly(bool readOnly)
{
    rootObject()->setProperty("readOnly", readOnly);
}

void BookmarkTooltip::onPlayClicked(int index)
{
    if (NX_ASSERT(index >= 0 && index < m_bookmarks.size()))
        emit playClicked(m_bookmarks.at(index));
}

void BookmarkTooltip::onEditClicked(int index)
{
    if (NX_ASSERT(index >= 0 && index < m_bookmarks.size()))
        emit editClicked(m_bookmarks.at(index));
}

void BookmarkTooltip::onExportClicked(int index)
{
    if (NX_ASSERT(index >= 0 && index < m_bookmarks.size()))
        emit exportClicked(m_bookmarks.at(index));
}

void BookmarkTooltip::onDeleteClicked(int index)
{
    if (NX_ASSERT(index >= 0 && index < m_bookmarks.size()))
        emit deleteClicked(m_bookmarks.at(index));
}

void BookmarkTooltip::resizeEvent(QResizeEvent* event)
{
    if (event->oldSize().isValid())
    {
        const auto yShift = event->size().height() - event->oldSize().height();
        move(x(), y() - yShift);

        // The resize event occurs when a user flips a bookmark. The bookmark flip leads to a mouse
        // leave event, which closes the tooltip. This workaround avoids this.
        installEventFilter(this);
    }

    base_type::resizeEvent(event);
}

bool BookmarkTooltip::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Leave)
    {
        // Filter was installed in the BookmarkTooltip::resizeEvent.
        removeEventFilter(this);
        return true;
    }

    return base_type::eventFilter(obj, event);
}

} // namespace nx::vms::client::desktop::workbench::timeline
