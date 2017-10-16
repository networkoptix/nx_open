#include "html.h"

#include <QtGui/QTextDocument>

#include <QtXml/QDomDocument>

#include <nx/utils/string.h>
#include <nx/utils/log/assert.h>

namespace {

QString replaceNewLineToBrTag(const QString &text)
{
    static const auto kNewLineSymbol = L'\n';
    static const auto kNewLineTag = lit("<br>");

    return text.trimmed().replace(kNewLineSymbol, kNewLineTag);
}

// @brief Tries to elide node text
// @return Resulting text length
int elideTextNode(QDomNode &node
                  , int maxLength
                  , const QString &/*tail*/)
{
    auto textNode = node.toText();
    if (textNode.isNull())
        return 0;

    const auto text = textNode.nodeValue();
    const auto textSize = text.size();
    if (textSize <= maxLength)
        return textSize;

    const auto elided = nx::utils::elideString(text, maxLength);
    textNode.setNodeValue(elided);
    return maxLength;
}

// @brief Tries to elide node (element) text.
// @return Resulting text length
int elideDomNode(QDomNode &node
                 , int maxLength
                 , const QString &tail)
{
    // if specified node is text - elide it
    if (auto len = elideTextNode(node, maxLength, tail))
        return len;

    int currentLength = 0;
    QList<QDomNode> forRemove;
    for (auto child = node.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        if (currentLength >= maxLength)
        {
            // Removes all elements that are not fit to length
            forRemove.append(child);
            continue;
        }

        // Tries to elide text node
        if (currentLength < maxLength)
        {
            if (auto length = elideTextNode(child, maxLength - currentLength, tail))
            {
                currentLength += length;
                continue;
            }
        }

        // if it is not text node, try to parse whole element
        const auto elem = child.toElement();
        if (elem.isNull())
        {
            // If it is not element (comment or some specific data) - skip it
            continue;
        }

        const int textLength = elem.text().size();
        if ((currentLength + textLength) <= maxLength)
        {
            // Text length of element (and all its child elements) is less then maximum
            currentLength += textLength;
            continue;
        }

        currentLength += elideDomNode(child, maxLength - currentLength, tail);
    }

    // Removes all elements that are not fit
    for (auto child : forRemove)
        node.removeChild(child);

    return currentLength;
};

} //anonymous namespace


QnHtmlTag::QnHtmlTag(const QLatin1String& tag, QString& result, LineBreaks lineBreaks) :
    m_tag(tag),
    m_result(result),
    m_lineBreaks(lineBreaks)
{
    result.append(lit("<%1>").arg(m_tag));
    if (m_lineBreaks.testFlag(AfterOpen))
        result.append(L'\n');
}

QnHtmlTag::QnHtmlTag(const char* tag, QString& result, LineBreaks lineBreaks) :
    QnHtmlTag(QLatin1String(tag), result, lineBreaks)
{}

QnHtmlTag::~QnHtmlTag()
{
    m_result.append(lit("</%1>").arg(m_tag));
    if (m_lineBreaks.testFlag(AfterClose))
        m_result.append(L'\n');
}

QString makeHtml(const QString& source)
{
    return lit("<html>") + source + lit("</html>");
}

QString htmlBold(const QString& source)
{
    return lit("<b>") + source + lit("</b>");
}

QString htmlFormattedParagraph(const QString &text, int pixelSize, bool isBold /*= false */, bool isItalic /*= false*/)
{
    static const auto kPTag = lit("<p style=\" text-indent: 0; font-size: %1px; font-weight: %2; font-style: %3; color: #FFF; margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; \">%4</p>");

    if (text.isEmpty())
        return QString();

    const QString boldValue = (isBold ? lit("bold") : lit("normal"));
    const QString italicValue(isItalic ? lit("italic") : lit("normal"));

    const auto newFormattedText = replaceNewLineToBrTag(text);
    return kPTag.arg(QString::number(pixelSize), boldValue, italicValue, newFormattedText);
}

QString makeHref(const QString& text, const QUrl& url)
{
    return makeHref(text, url.toString());
}

QString makeHref(const QString& text, const QString& link)
{
    return lit("<a href=\"%2\">%1</a>").arg(text, link);
}

QString escapeHtml(const QString& input)
{
    QString escaped(input);
    for (int i = 0; i < escaped.count();)
    {
        const char* replacement = 0;
        ushort ch = escaped.at(i).unicode();
        if (ch == '&')
        {
            replacement = "&amp;";
        }
        else if (ch == '<')
        {
            replacement = "&lt;";
        }
        else if (ch == '>')
        {
            replacement = "&gt;";
        }
        else if (ch == '"')
        {
            replacement = "&quot;";
        }
        if (replacement)
        {
            escaped.replace(i, 1, QLatin1String(replacement));
            i += static_cast<int>(strlen(replacement));
        }
        else
        {
            ++i;
        }
    }
    return escaped;
}

QString unescapeHtml(const QString& escaped)
{
    QString unescaped(escaped);
    unescaped.replace(QLatin1String("&lt;"), QLatin1String("<"));
    unescaped.replace(QLatin1String("&gt;"), QLatin1String(">"));
    unescaped.replace(QLatin1String("&amp;"), QLatin1String("&"));
    unescaped.replace(QLatin1String("&quot;"), QLatin1String("\""));
    return unescaped;
}

QString elideHtml(const QString &html, int maxLength, const QString &tail)
{
    QDomDocument dom;
    dom.setContent(replaceNewLineToBrTag(html));
    auto root = dom.documentElement();
    elideDomNode(root, maxLength, tail);
    return dom.toString();
}

bool mightBeHtml(const QString& text)
{
    if (!text.contains(L'\n'))
        return Qt::mightBeRichText(text);

    return mightBeHtml(text.split(L'\n'));
}

bool mightBeHtml(const QStringList& lines)
{
    return std::any_of(lines.cbegin(), lines.cend(),
        [](const QString& line) { return mightBeHtml(line); });
}

QString ensureHtml(const QString& source)
{
    return mightBeHtml(source)
        ? source
        : Qt::convertFromPlainText(source);
}
