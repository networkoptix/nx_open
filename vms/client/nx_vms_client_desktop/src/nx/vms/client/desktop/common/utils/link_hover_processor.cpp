#include "link_hover_processor.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

#include <ui/style/helper.h>
#include <ui/workaround/label_link_tabstop_workaround.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/event_processors.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

namespace nx::vms::client::desktop {

LinkHoverProcessor::LinkHoverProcessor(QLabel* parent):
    QObject(parent),
    m_label(parent)
{
    NX_ASSERT(parent);
    if (!parent)
        return;

    m_label->setAttribute(Qt::WA_Hover);
    m_originalText = m_label->text();
    m_alteredText = m_originalText;

    updateColors(UpdateTime::Now);

    const auto handledEvents = {
        QEvent::UpdateRequest,
        QEvent::Show,
        QEvent::HoverLeave,
        QEvent::MouseButtonRelease };


    // m_label will be destroyed before 'this'.
    // If m_label is a signal reciever, event queue would be cleaned after it's destructions.
    installEventHandler(m_label, handledEvents, m_label,
        [this](QObject* /*object*/, QEvent* event)
        {
            switch (event->type())
            {
                case QEvent::UpdateRequest:
                case QEvent::Show:
                {
                    if (updateOriginalText())
                        updateColors(UpdateTime::Now);
                    break;
                }

                case QEvent::HoverLeave:
                {
                    // QLabel and underlying QWidgetTextControl handle only MouseMove event.
                    // So if the mouse leaves control, they don't emit linkHovered(L"").
                    // Worse, they still consider last hovered link hovered, so if the mouse
                    // enters control and hovers the link again, they don't emit linkHovered(link)
                    // To fix that we send fake MouseMove upon leaving the control:
                    const int kFarFarAway = -1.0e8;
                    QMouseEvent kFakeMouseMove(QEvent::MouseMove, QPointF(kFarFarAway, kFarFarAway),
                        Qt::NoButton, 0, 0);

                    QnScopedTypedPropertyRollback<bool, QLabel> propagationGuard(
                        m_label,
                        [](QLabel* label, bool value) { label->setAttribute(Qt::WA_NoMousePropagation, value); },
                        [](const QLabel* label) { return label->testAttribute(Qt::WA_NoMousePropagation); },
                        true);

                    QApplication::sendEvent(m_label, &kFakeMouseMove);
                    break;
                }

                case QEvent::MouseButtonRelease:
                {
                    // QLabel ignores MouseButtonPress events, but keeps MouseButtonRelease events
                    // accepted if it contains hypertext links. To fix that (parent receiving
                    // presses but not releases) we need to accept events only if link is hovered.
                    event->setAccepted(!m_hoveredLink.isEmpty());
                    break;
                }

                default:
                    break;
            }
        });

    auto tabstopListener = new QnLabelFocusListener(this);
    m_label->installEventFilter(tabstopListener);

    connect(m_label, &QLabel::linkHovered, this, &LinkHoverProcessor::linkHovered);
}

bool LinkHoverProcessor::updateOriginalText()
{
    const QString text = m_label->text();
    if (m_alteredText == text)
        return false;

    m_originalText = text;
    return true;
}

void LinkHoverProcessor::changeLabelState(const QString& text, bool hovered)
{
    m_alteredText = text;

    m_label->setText(text);

    if (!WidgetUtils::graphicsProxyWidget(m_label))
        m_label->repaint();

    if (hovered)
        m_label->setCursor(Qt::PointingHandCursor);
    else
        m_label->unsetCursor();
}

void LinkHoverProcessor::linkHovered(const QString& href)
{
    m_hoveredLink = href;
    updateOriginalText();
    updateColors(UpdateTime::Later);
}

void LinkHoverProcessor::updateColors(UpdateTime when)
{
    /* Find anchor position: */
    const bool hovered = !m_hoveredLink.isEmpty();
    const int linkPos = hovered ? m_originalText.indexOf(m_hoveredLink) : -1;
    const int hoveredHrefPos = hovered
        ? m_originalText.lastIndexOf(lit("href"), linkPos, Qt::CaseInsensitive)
        : -1;

    /* Insert color attribute before each href attribute: */
    QString alteredText = m_originalText;
    for (int pos = -1;;)
    {
        /* Find href attribute position: */
        pos = alteredText.lastIndexOf(lit("href"), pos, Qt::CaseInsensitive);
        if (pos == -1)
            break;

        // TODO: #common #vkutin Implement a better parsing for this to work if "style" attribute already exists
        const QColor color = style::linkColor(m_label->palette(), pos == hoveredHrefPos);
        alteredText.insert(pos, lit("style='color: %1' ").arg(color.name(QColor::HexRgb)));
    }

    /* Apply altered label text: */
    if (when == UpdateTime::Now)
    {
        changeLabelState(alteredText, hovered);
    }
    else
    {
        auto changer = [this, alteredText, hovered]() { changeLabelState(alteredText, hovered); };
        executeLater(changer, this);
    }
}

} // namespace nx::vms::client::desktop
