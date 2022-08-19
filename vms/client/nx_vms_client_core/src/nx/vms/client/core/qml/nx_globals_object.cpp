// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_globals_object.h"

#include <QtCore/QFile>
#include <QtCore/QtMath>
#include <QtCore/QRegularExpression>
#include <QtGui/QClipboard>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQuick/private/qquickitem_p.h>

#include <nx/vms/common/html/html.h>

namespace nx::vms::client::core {

namespace detail {

QQuickFlickable* findFlickable(const QQuickItem* item)
{
    if (!item)
        return nullptr;

    auto parent = item->parentItem();
    while (parent)
    {
        if (auto flickable = qobject_cast<QQuickFlickable*>(parent))
            return flickable;

        parent = parent->parentItem();
    }

    return nullptr;
}

bool itemIsAncestorOf(QQuickItem* item, QQuickItem* parent)
{
    if (!item || !parent)
        return false;

    while ((item = item->parentItem()))
    {
        if (item == parent)
            return true;
    }

    return false;
}

} using namespace detail;

NxGlobalsObject::NxGlobalsObject(QObject* parent):
    QObject(parent)
{
}

utils::Url NxGlobalsObject::url(const QString& url) const
{
    return utils::Url(url);
}

utils::Url NxGlobalsObject::url(const QUrl& url) const
{
    return utils::Url::fromQUrl(url);
}

nx::utils::Url NxGlobalsObject::urlFromUserInput(const QString& url) const
{
    return utils::Url::fromUserInput(url);
}

nx::utils::Url NxGlobalsObject::emptyUrl() const
{
    return utils::Url();
}

QModelIndex NxGlobalsObject::invalidModelIndex() const
{
    return {};
}

QPersistentModelIndex NxGlobalsObject::toPersistent(const QModelIndex& index) const
{
    return QPersistentModelIndex(index);
}

QVariantList NxGlobalsObject::toVariantList(const QModelIndexList& indexList) const
{
    QVariantList result;
    result.reserve(indexList.size());
    std::copy(indexList.cbegin(), indexList.cend(), std::back_inserter(result));
    return result;
}

nx::vms::api::SoftwareVersion NxGlobalsObject::softwareVersion(const QString& version) const
{
    return nx::vms::api::SoftwareVersion(version);
}

bool NxGlobalsObject::ensureFlickableChildVisible(QQuickItem* item)
{
    if (!item)
        return false;

    auto flickable = findFlickable(item);
    if (!flickable)
        return false;

    static const auto kDenyPositionCorrectionPropertyName = "denyFlickableVisibleAreaCorrection";
    const auto denyCorrection = flickable->property(kDenyPositionCorrectionPropertyName);
    if (denyCorrection.isValid() && denyCorrection.toBool())
        return false;

    const auto contentItem = flickable->contentItem();
    if (!contentItem || !itemIsAncestorOf(item, contentItem))
        return false;

    const auto rect = item->mapRectToItem(contentItem,
        QRect(0, 0, static_cast<int>(item->width()), static_cast<int>(item->height())));

    auto adjustContentPosition =
        [](qreal position, qreal origin, qreal contentSize, qreal flickableSize,
            qreal startMargin, qreal endMargin,
            qreal itemPosition, qreal itemSize)
        {
            const auto itemEnd = itemPosition + itemSize - position;
            if (itemEnd > flickableSize)
                position += (itemEnd - flickableSize);

            const auto itemStart = itemPosition - position;
            if (itemStart < 0)
                position += itemStart;

            position = qBound(-startMargin, position, contentSize - flickableSize + endMargin);

            return position + origin;
        };

    flickable->setContentX(adjustContentPosition(
        flickable->contentX(), flickable->originX(),
        flickable->contentWidth(), flickable->width(),
        flickable->leftMargin(), flickable->rightMargin(),
        rect.x(), rect.width()));
    flickable->setContentY(adjustContentPosition(
        flickable->contentY(), flickable->originY(),
        flickable->contentHeight(), flickable->height(),
        flickable->topMargin(), flickable->bottomMargin(),
        rect.y(), rect.height()));

    return true;
}

QnUuid NxGlobalsObject::uuid(const QString& uuid) const
{
    return QnUuid::fromStringSafe(uuid);
}

QnUuid NxGlobalsObject::generateUuid() const
{
    return QnUuid::createUuid();
}

bool NxGlobalsObject::fileExists(const QString& path) const
{
    return QFile::exists(path);
}

QLocale NxGlobalsObject::numericInputLocale(const QString& basedOn) const
{
    auto locale = basedOn.isEmpty() ? QLocale() : QLocale(basedOn);
    locale.setNumberOptions(QLocale::RejectGroupSeparator | QLocale::OmitGroupSeparator);
    return locale;
}

QCursor NxGlobalsObject::cursor(Qt::CursorShape shape) const
{
    return QCursor(shape);
}

bool NxGlobalsObject::mightBeHtml(const QString& text) const
{
    return common::html::mightBeHtml(text);
}

bool NxGlobalsObject::isRelevantForPositioners(QQuickItem* item) const
{
    return NX_ASSERT(item)
        ? !QQuickItemPrivate::get(item)->isTransparentForPositioner()
        : false;
}

void NxGlobalsObject::copyToClipboard(const QString& text) const
{
    qApp->clipboard()->setText(text);
}

double NxGlobalsObject::toDouble(const QVariant& value) const
{
    return value.toDouble();
}

QString NxGlobalsObject::makeSearchRegExp(const QString& value) const
{
    static const auto kEscapedStar = QRegularExpression::escape("*");
    static const auto kEscapedQuestionMark = QRegularExpression::escape("?");
    static const auto kRegExpStar = ".*";
    static const auto kRegExpQuestionMark = ".";

    auto result = QRegularExpression::escape(value);
    result.replace(kEscapedStar, kRegExpStar);
    result.replace(kEscapedQuestionMark, kRegExpQuestionMark);
    return QRegularExpression::anchoredPattern(result);
}

QString NxGlobalsObject::escapeRegExp(const QString& value) const
{
    return QRegularExpression::escape(value);
}

} // namespace nx::vms::client::core
