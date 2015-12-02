
#include "bookmark_tags_control.h"

namespace
{
    static const auto kHtmlTemplate = lit("<html><body>%1</body></html>");
    static const auto kSpacerTag = lit("<span style= \"font-size:13px; vertical-align: middle\">&nbsp;<span style= \"vertical-align: middle; font-size:11px; font-weight: bold; color: white\">%1</span>&nbsp;</span></span>");
    static const auto kTableTag = lit("<table cellspacing = \"1\" cellpadding=\"3\" style = \"margin-top: 0;float: left;display:inline-block; border-style: none;border-width:0;\"><tr><td bgcolor = %1><a href = \"%2\" style = \"text-decoration: none;\">%3</a></td></tr></table></tr></table>");
}

QnBookmarkTagsControl::QnBookmarkTagsControl(const QnCameraBookmarkTags &tags
    , QGraphicsItem *parent)
    : QnProxyLabel(parent)
    , m_colors()
    , m_tags()
{
    setAcceptHoverEvents(true);

    setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextEditable);
    connect(this, &QnBookmarkTagsControl::linkHovered, this, &QnBookmarkTagsControl::onLinkHovered);
    connect(this, &QnBookmarkTagsControl::linkActivated, this, &QnBookmarkTagsControl::tagClicked);

    /* For future debug
    connect(this, &QnTagsControl::linkActivated, this, [this](const QString &link)
    {
        qDebug() <<link;
    });
    */

    setTags(tags);
}

QnBookmarkTagsControl::~QnBookmarkTagsControl()
{

}

void QnBookmarkTagsControl::setTags(const QnCameraBookmarkTags &tags)
{
    if (m_tags == tags)
        return;

    m_tags = tags;

    updateCurrentTag(QString());
}

void QnBookmarkTagsControl::updateCurrentTag(const QString &currentTag)
{
    setCursor(currentTag.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);

    QString tagsText;
    for (const auto &tag: m_tags)
    {
        const auto color = (tag == currentTag ? m_colors.tagBgHovered.name(QColor::HexRgb)
            : m_colors.tagBgNormal.name(QColor::HexRgb));
        tagsText += kTableTag.arg(color, tag, kSpacerTag.arg(tag));
    }

    const auto labelText = (tagsText.isEmpty() ? QString() : kHtmlTemplate.arg(tagsText));
    setText(labelText);
}

const QnBookmarkColors &QnBookmarkTagsControl::colors() const
{
    return m_colors;
}

void QnBookmarkTagsControl::setColors(const QnBookmarkColors &colors)
{
    if (colors == m_colors)
        return;

    m_colors = colors;
}

void QnBookmarkTagsControl::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
    updateCurrentTag(QString());
    QnProxyLabel::hoverLeaveEvent(event);
}

void QnBookmarkTagsControl::onLinkHovered(const QString &tag)
{
    updateCurrentTag(tag);
}
