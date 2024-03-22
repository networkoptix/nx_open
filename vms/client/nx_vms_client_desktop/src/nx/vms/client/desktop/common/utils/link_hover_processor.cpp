// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "link_hover_processor.h"

#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtGui/QMouseEvent>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>
#include <QtGui/QTextFragment>
#include <QtGui/QTextFrame>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/common/html/html.h>
#include <ui/workaround/label_link_tabstop_workaround.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/event_processors.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

namespace nx::vms::client::desktop {

namespace {

// Check whether a label is not fully or partially destroyed.
// It will return false if called from inside ~QFrame, ~QWidget or ~QObject
// (though it will return true if called from inside ~QLabel).
bool isAlive(const QPointer<QLabel>& label)
{
    return label && label->metaObject()->inherits(&QLabel::staticMetaObject);
}

Qt::TextFormat labelFormat(QLabel* label)
{
    const auto format = label->textFormat();
    if (format != Qt::AutoText)
        return format;

    return common::html::mightBeHtml(label->text())
        ? Qt::RichText
        : Qt::PlainText;
}

void changeForeground(QTextCursor& cursor, const QColor& color)
{
    QTextCharFormat format;
    format.setForeground(color);
    cursor.mergeCharFormat(format);
}

} // namespace

struct LinkHoverProcessor::Private
{
    LinkHoverProcessor* const q;
    const QPointer<QLabel> label;
    QString alteredText;
    QString hoveredLink;
    QTextDocument textDocument{};
    QTextCursor hoveredLinkCursor;
    QHash<QString, QTextCursor> linkCursors;

    void handleLinkHovered(const QString& href);
    void changeLabelState(const QString& text, bool hovered);
    void checkForOriginalTextChange();
};

LinkHoverProcessor::LinkHoverProcessor(QLabel* parent):
    QObject(parent),
    d(new Private{.q = this, .label = parent})
{
    NX_ASSERT(parent);
    if (!parent)
        return;

    d->label->setAttribute(Qt::WA_Hover);
    d->checkForOriginalTextChange();

    const auto handledEvents = {
        QEvent::UpdateRequest,
        QEvent::UpdateLater,
        QEvent::Show,
        QEvent::HoverLeave,
        QEvent::MouseButtonRelease };

    installEventHandler(d->label, handledEvents, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            if (!isAlive(d->label))
                return;

            switch (event->type())
            {
                case QEvent::UpdateRequest:
                case QEvent::UpdateLater:
                case QEvent::Show:
                {
                    d->checkForOriginalTextChange();
                    break;
                }

                case QEvent::HoverLeave:
                {
                    // QLabel and underlying QWidgetTextControl handle only MouseMove event.
                    // So if the mouse leaves control, they don't emit linkHovered("").
                    // Worse, they still consider last hovered link hovered, so if the mouse
                    // enters control and hovers the link again, they don't emit linkHovered(link)
                    // To fix that we send fake MouseMove upon leaving the control:
                    const int kFarFarAway = -1.0e8;
                    QMouseEvent kFakeMouseMove(QEvent::MouseMove, QPointF(kFarFarAway, kFarFarAway),
                        Qt::NoButton, Qt::NoButton, Qt::NoModifier);

                    QnScopedTypedPropertyRollback<bool, QLabel> propagationGuard(
                        d->label,
                        [](QLabel* label, bool value) { label->setAttribute(Qt::WA_NoMousePropagation, value); },
                        [](const QLabel* label) { return label->testAttribute(Qt::WA_NoMousePropagation); },
                        true);

                    QApplication::sendEvent(d->label, &kFakeMouseMove);
                    break;
                }

                case QEvent::MouseButtonRelease:
                {
                    // QLabel ignores MouseButtonPress events, but keeps MouseButtonRelease events
                    // accepted if it contains hypertext links. To fix that (parent receiving
                    // presses but not releases) we need to accept events only if link is hovered.
                    event->setAccepted(!d->hoveredLink.isEmpty());
                    break;
                }

                default:
                    break;
            }
        });

    auto tabstopListener = new QnLabelFocusListener(this);
    d->label->installEventFilter(tabstopListener);

    connect(d->label, &QLabel::linkHovered, this,
        [this](const QString& link) { d->handleLinkHovered(link); });
}

LinkHoverProcessor::~LinkHoverProcessor()
{
    // Required here for forward-declared scoped pointer destruction.
}

void LinkHoverProcessor::Private::checkForOriginalTextChange()
{
    if (!NX_ASSERT(isAlive(label)))
        return;

    const QString text = label->text();
    if (alteredText == text)
        return;

    label->unsetCursor();

    if (labelFormat(label) == Qt::RichText)
        textDocument.setHtml(text);
    else
        textDocument.clear();

    hoveredLinkCursor = {};
    hoveredLink.clear();
    linkCursors.clear();

    // Bypass empty and plain text labels.
    if (textDocument.isEmpty())
        return;

    // Find all hyperlinks.
    for (auto block = textDocument.begin(); block != textDocument.end(); block = block.next())
    {
        if (!block.isValid())
            continue;

        for (auto it = block.begin(); !it.atEnd(); ++it)
        {
            const auto fragment = it.fragment();
            if (fragment.isValid())
            {
                const auto charFormat = fragment.charFormat();
                if (charFormat.isAnchor())
                {
                    QTextCursor cursor(&textDocument);
                    cursor.setPosition(fragment.position(), QTextCursor::MoveAnchor);
                    cursor.setPosition(fragment.position() + fragment.length(),
                        QTextCursor::KeepAnchor);
                    linkCursors[charFormat.anchorHref()] = cursor;
                }
            }
        }
    }

    // Bypass labels without hyperlinks.
    if (linkCursors.empty())
        return;

    // Force initial not hovered color for all hyperlinks.
    for (auto& linkCursor: linkCursors)
        changeForeground(linkCursor, style::linkColor(label->palette(), /*hovered*/ false));

    changeLabelState(textDocument.toHtml(), /*hovered*/ false);
}

void LinkHoverProcessor::Private::changeLabelState(const QString& text, bool hovered)
{
    if (!isAlive(label))
        return;

    alteredText = text;
    label->setText(text);

    if (hovered)
        label->setCursor(Qt::PointingHandCursor);
    else
        label->unsetCursor();
}

void LinkHoverProcessor::Private::handleLinkHovered(const QString& href)
{
    if (!NX_ASSERT(isAlive(label)))
        return;

    hoveredLink = href;
    checkForOriginalTextChange();

    if (textDocument.isEmpty())
        return;

    if (!hoveredLinkCursor.isNull())
        changeForeground(hoveredLinkCursor, style::linkColor(label->palette(), /*hovered*/ false));

    hoveredLinkCursor = linkCursors.value(hoveredLink);

    if (!hoveredLinkCursor.isNull())
        changeForeground(hoveredLinkCursor, style::linkColor(label->palette(), /*hovered*/ true));

    const bool hovered = !hoveredLink.isEmpty();
    const auto alteredText = textDocument.toHtml();

    auto changer =
        [this, alteredText, hovered]() { changeLabelState(alteredText, hovered); };

    executeLater(changer, q);
}

} // namespace nx::vms::client::desktop
