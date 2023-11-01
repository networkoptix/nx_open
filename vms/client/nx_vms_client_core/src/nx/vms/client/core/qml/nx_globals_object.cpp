// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_globals_object.h"

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QtMath>
#include <QtGui/QClipboard>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQuick/private/qquickitem_p.h>

#include <nx/build_info.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/synctime.h>

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

QModelIndex NxGlobalsObject::fromPersistent(const QPersistentModelIndex& index) const
{
    return index;
}

QVariant NxGlobalsObject::modelData(const QModelIndex& index, const QString& roleName) const
{
    if (!index.isValid())
        return {};

    constexpr int kNoRole = -1;
    const int role = index.model()->roleNames().key(roleName.toUtf8(), kNoRole);
    if (!NX_ASSERT(role != kNoRole, "Role '%1' not found", roleName))
        return {};

    return index.data(role);
}

bool NxGlobalsObject::isRecursiveChildOf(const QModelIndex& child, const QModelIndex& parent) const
{
    if (!child.isValid()
        || child == parent //< We don't consider an index a recursive child of itself.
        || !NX_ASSERT(parent.isValid())
        || !NX_ASSERT(child.model() == parent.model()))
    {
        return false;
    }

    const auto next = child.parent();
    return next == parent
        ? true
        : isRecursiveChildOf(next, parent);
}

bool NxGlobalsObject::hasChildren(const QModelIndex& index) const
{
    return index.isValid() && index.model()->hasChildren(index);
}

Qt::ItemFlags NxGlobalsObject::itemFlags(const QModelIndex& index) const
{
    return index.isValid()
        ? index.model()->flags(index)
        : Qt::ItemFlags{};
}

QVariantList NxGlobalsObject::toQVariantList(const QModelIndexList& indexList) const
{
    return nx::utils::toQVariantList(indexList);
}

nx::utils::SoftwareVersion NxGlobalsObject::softwareVersion(const QString& version) const
{
    return nx::utils::SoftwareVersion(version);
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
            if (contentSize < flickableSize)
                return position;

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

DateRange NxGlobalsObject::dateRange(const QDateTime& start, const QDateTime& end) const
{
    return DateRange{start, end};
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

QString NxGlobalsObject::makeSearchRegExpNoAnchors(const QString& value) const
{
    static const auto kEscapedStar = QRegularExpression::escape("*");
    static const auto kEscapedQuestionMark = QRegularExpression::escape("?");
    static const auto kRegExpStar = ".*";
    static const auto kRegExpQuestionMark = ".";

    auto result = QRegularExpression::escape(value);
    result.replace(kEscapedStar, kRegExpStar);
    result.replace(kEscapedQuestionMark, kRegExpQuestionMark);
    return result;
}

QString NxGlobalsObject::makeSearchRegExp(const QString& value) const
{
    return QRegularExpression::anchoredPattern(makeSearchRegExpNoAnchors(value));
}

QString NxGlobalsObject::escapeRegExp(const QString& value) const
{
    return QRegularExpression::escape(value);
}

void NxGlobalsObject::invokeMethod(QObject* obj, const QString& methodName)
{
    QMetaObject::invokeMethod(obj, methodName.toLatin1().constData(), Qt::DirectConnection);
}

QString NxGlobalsObject::modifierName(const Qt::KeyboardModifier modifier) const
{
    if (nx::build_info::isMacOsX())
    {
        switch (modifier)
        {
            case Qt::ShiftModifier:
                return nx::UnicodeChars::kUpwardsWhiteArrow;

            case Qt::AltModifier:
                return nx::UnicodeChars::kOptionKey;

            case Qt::ControlModifier:
                return nx::UnicodeChars::kCommandKey;

            case Qt::MetaModifier:
                return nx::UnicodeChars::kUpArrowhead;

            default:
                return "";
        }
    }

    switch (modifier)
    {
        case Qt::ShiftModifier:
            return "Shift";

        case Qt::AltModifier:
            return "Alt";

        case Qt::ControlModifier:
            return "Ctrl";

        case Qt::MetaModifier:
            return nx::build_info::isWindows() ? "Win" : "Meta";

        default:
            return "";
    }
}

QString NxGlobalsObject::dateInShortFormat(const QDateTime& date)
{
    return nx::vms::time::toString(date, nx::vms::time::Format::dd_MM_yyyy);
}

QString NxGlobalsObject::dateTimeInShortFormat(const QDateTime& dateTime)
{
    return nx::vms::time::toString(dateTime, nx::vms::time::Format::dd_MM_yyyy_hh_mm_ss);
}

QString NxGlobalsObject::highlightMatch(
    const QString& text, const QRegularExpression& rx, const QColor& color) const
{
    return common::html::highlightMatch(text, rx, color);
}

QString NxGlobalsObject::toHtmlEscaped(const QString& text) const
{
    return common::html::toHtmlEscaped(text);
}

qint64 NxGlobalsObject::syncTimeCurrentTimePointMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(qnSyncTime->currentTimePoint()).count();
}

QString NxGlobalsObject::shortcutText(const QVariant& var) const
{
    const QKeySequence sequence = var.metaType().id() == QMetaType::Int
        ? QKeySequence(static_cast<QKeySequence::StandardKey>(var.toInt()))
        : QKeySequence::fromString(var.toString());

    return sequence.toString(QKeySequence::NativeText);
}

} // namespace nx::vms::client::core
