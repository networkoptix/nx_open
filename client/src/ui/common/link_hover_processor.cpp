#include "link_hover_processor.h"

#include <nx/utils/log/assert.h>

#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>


QnLinkHoverProcessor::QnLinkHoverProcessor(QLabel* parent) :
    QObject(parent),
    m_label(parent),
    m_textAltered(false)
{
    NX_ASSERT(parent);
    if (!parent)
        return;

    m_label->setAttribute(Qt::WA_Hover);
    m_originalText = m_label->text();

    auto hoverSignalizer = new QnMultiEventSignalizer(this);
    hoverSignalizer->addEventType(QEvent::HoverEnter);
    hoverSignalizer->addEventType(QEvent::HoverLeave);
    hoverSignalizer->addEventType(QEvent::HoverMove);
    m_label->installEventFilter(hoverSignalizer);

    connect(hoverSignalizer, &QnMultiEventSignalizer::activated, this, [this](QObject* object, QEvent* event)
    {
        Q_UNUSED(object);
        switch (event->type())
        {
            case QEvent::HoverEnter:
            case QEvent::HoverMove:
            {
                if (!m_textAltered)
                    m_originalText = m_label->text();
                break;
            }

            case QEvent::HoverLeave:
            {
                /*
                 * QLabel and underlying QWidgetTextControl handle only MouseMove event.
                 * So if the mouse leaves control, they don't emit linkHovered(L"").
                 * Worse, they still consider last hovered link hovered, so if the mouse
                 * enters control and hovers the link again, they don't emit linkHovered(link)
                 * To fix that we send fake MouseMove upon leaving the control:
                 */
                const int kFarFarAway = -1.0e8;
                QMouseEvent kFakeMouseMove(QEvent::MouseMove, QPointF(kFarFarAway, kFarFarAway), Qt::NoButton, 0, 0);
                QApplication::sendEvent(m_label, &kFakeMouseMove);
                break;
            }

            default:
                break;
        }
    });

    connect(m_label, &QLabel::linkHovered, this, &QnLinkHoverProcessor::linkHovered);
}

auto QnLinkHoverProcessor::labelChangeFunctor(const QString& text, bool hovered)
{
    return [this, text, hovered]()
    {
        m_label->setText(text);
        m_label->repaint();
        m_textAltered = hovered;

        if (hovered)
            m_label->setCursor(Qt::PointingHandCursor);
        else
            m_label->unsetCursor();
    };
}

QColor QnLinkHoverProcessor::hoveredColor() const
{
    return m_label->palette().color(QPalette::Link);
}

void QnLinkHoverProcessor::linkHovered(const QString& href)
{
    if (href.isEmpty())
    {
        /* If link was unhovered, restore unaltered label text: */
        executeDelayedParented(labelChangeFunctor(m_originalText, false), 0, this);
        return;
    }

    /* Find anchor position: */
    int pos = m_originalText.indexOf(href);
    if (pos == -1)
        return;

    /* Find href attribute position: */
    pos = m_originalText.lastIndexOf(lit("href"), pos, Qt::CaseInsensitive);
    if (pos == -1)
        return;

    /* Insert color attribute before href attribute: */
    QString alteredText = m_originalText;
    QString colorString = hoveredColor().name(QColor::HexRgb);
    alteredText.insert(pos, lit("style='color: %1' ").arg(colorString));

    /* Apply altered label text: */
    executeDelayedParented(labelChangeFunctor(alteredText, true), 0, this);
}

