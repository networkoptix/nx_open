#pragma once

#include <QtCore/QString>
#include <QtGui/QColor>

#include <nx/utils/url.h>

namespace nx::vms::common {
namespace html {

QString colored(const QString& text, const QColor& color);

} // namespace html
} // namespace nx::vms::common

/** RAII helper to consistently open/close html tags on the given string. */
class QnHtmlTag
{
public:
    enum LineBreak
    {
        NoBreaks = 0x00,
        AfterOpen = 0x01,
        AfterClose = 0x02,
        BothBreaks = AfterOpen | AfterClose
    };
    Q_DECLARE_FLAGS(LineBreaks, LineBreak)

    QnHtmlTag(const QLatin1String& tag, QString& result, LineBreaks lineBreaks = AfterClose);
    QnHtmlTag(const char* tag, QString& result, LineBreaks lineBreaks = AfterClose);
    ~QnHtmlTag();
private:
    QLatin1String m_tag;
    QString& m_result;
    LineBreaks m_lineBreaks;
};

QString makeHtml(const QString& source);
QString htmlBold(const QString& source);
QString htmlFormattedParagraph(const QString &text
    , int pixelSize
    , bool isBold = false
    , bool isItalic = false);

QString htmlParagraph(const QString& source);

/**
 * Create html link with text, equal to url.
 */
QString makeHref(const QUrl& url);

/**
 * Create html link with text, equal to url.
 */
QString makeHref(const nx::utils::Url& url);

/**
 * Create html link with text, equal to url.
 */
QString makeHref(const QString& url);

/**
 * Create html link with the given text and url.
 */
QString makeHref(const QString& text, const QUrl& url);

/**
 * Create html link with the given text and url.
 */
QString makeHref(const QString& text, const nx::utils::Url& url);

/**
 * Create html link with the given text and url.
 */
QString makeHref(const QString& text, const QString& link);

QString makeMailHref(const QString& text);
QString makeMailHref(const QString& text, const QString& mail);
QString makeMailHref(const QString& text, const QUrl& mail);


QString escapeHtml(const QString& input);

QString unescapeHtml(const QString& escaped);

QString elideHtml(const QString &html, int maxLength, const QString &tail = QStringLiteral("..."));

bool mightBeHtml(const QString& text);
bool mightBeHtml(const QStringList& lines);

QString ensureHtml(const QString& source); //< Converts source to HTML if it is plain.
