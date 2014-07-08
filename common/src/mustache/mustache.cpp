/*
  Copyright 2012, Robert Knight

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
*/
#include "mustache.h"

#ifdef ENABLE_SENDMAIL

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

#include <common/common_globals.h>

using namespace Mustache;

QString Mustache::renderTemplate(const QString& templateString, const QVariant& args)
{
    Mustache::QtVariantContext context(args);
    Mustache::Renderer renderer;
    return renderer.render(templateString, &context);
}

QString escapeHtml(const QString& input)
{
    QString escaped(input);
    for (int i=0; i < escaped.count();) {
        const char* replacement = 0;
        ushort ch = escaped.at(i).unicode();
        if (ch == '&') {
            replacement = "&amp;";
        } else if (ch == '<') {
            replacement = "&lt;";
        } else if (ch == '>') {
            replacement = "&gt;";
        } else if (ch == '"') {
            replacement = "&quot;";
        }
        if (replacement) {
            escaped.replace(i, 1, QLatin1String(replacement));
            i += strlen(replacement);
        } else {
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

Context::Context(PartialResolver* resolver)
    : m_partialResolver(resolver)
{}

PartialResolver* Context::partialResolver() const
{
    return m_partialResolver;
}

QString Context::partialValue(const QString& key) const
{
    if (!m_partialResolver) {
        return QString();
    }
    return m_partialResolver->getPartial(key);
}

bool Context::canEval(const QString&) const
{
    return false;
}

QString Context::eval(const QString& key, const QString& _template, Renderer* renderer)
{
    Q_UNUSED(key);
    Q_UNUSED(_template);
    Q_UNUSED(renderer);

    return QString();
}

QtVariantContext::QtVariantContext(const QVariant& root, PartialResolver* resolver)
    : Context(resolver)
{
    m_contextStack << root;
}

QVariant variantMapValue(const QVariant& value, const QString& key)
{
    if (value.userType() == QVariant::Map) {
        return value.toMap().value(key);
    } else {
        return value.toHash().value(key);
    }
}

QVariant QtVariantContext::value(const QString& key) const
{
    for (int i = m_contextStack.count()-1; i >= 0; i--) {
        QVariant value = variantMapValue(m_contextStack.at(i), key);
        if (!value.isNull()) {
            return value;
        }
    }
    return QVariant();
}

bool QtVariantContext::isFalse(const QString& key) const
{
    QVariant value = this->value(key);
    switch (value.userType()) {
    case QVariant::Bool:
        return !value.toBool();
    case QVariant::List:
        return value.toList().isEmpty();
    default:
        return value.toString().isEmpty();
    }
}

QString QtVariantContext::stringValue(const QString& key) const
{
    if (isFalse(key)) {
        return QString();
    }
    return value(key).toString();
}

void QtVariantContext::push(const QString& key, int index)
{
    QVariant mapItem = value(key);
    if (index == -1) {
        m_contextStack << mapItem;
    } else {
        QVariantList list = mapItem.toList();
        m_contextStack << list.value(index, QVariant());
    }
}

void QtVariantContext::pop()
{
    m_contextStack.pop();
}

int QtVariantContext::listCount(const QString& key) const
{
    if (value(key).userType() == QVariant::List) {
        return value(key).toList().count();
    }
    return 0;
}

PartialMap::PartialMap(const QHash<QString, QString>& partials)
    : m_partials(partials)
{}

QString PartialMap::getPartial(const QString& name)
{
    return m_partials.value(name);
}

PartialFileLoader::PartialFileLoader(const QString& basePath)
    : m_basePath(basePath)
{}

QString PartialFileLoader::getPartial(const QString& name)
{
    if (!m_cache.contains(name)) {
        QString path = m_basePath + L'/' + name + lit(".mustache");
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream stream(&file);
            m_cache.insert(name, stream.readAll());
        }
    }
    return m_cache.value(name);
}

Renderer::Renderer()
    : m_errorPos(-1)
    , m_defaultTagStartMarker(lit("{{"))
    , m_defaultTagEndMarker(lit("}}"))
{
}

QString Renderer::error() const
{
    return m_error;
}

int Renderer::errorPos() const
{
    return m_errorPos;
}

QString Renderer::errorPartial() const
{
    return m_errorPartial;
}

QString Renderer::render(const QString& _template, Context* context)
{
    m_error.clear();
    m_errorPos = -1;
    m_errorPartial.clear();

    m_tagStartMarker = m_defaultTagStartMarker;
    m_tagEndMarker = m_defaultTagEndMarker;

    return render(_template, 0, _template.length(), context);
}

QString Renderer::render(const QString& _template, int startPos, int endPos, Context* context)
{
    QString output;
    int lastTagEnd = startPos;

    while (m_errorPos == -1) {
        Tag tag = findTag(_template, lastTagEnd, endPos);
        if (tag.type == Tag::Null) {
            output += _template.midRef(lastTagEnd, endPos - lastTagEnd);
            break;
        }
        output += _template.midRef(lastTagEnd, tag.start - lastTagEnd);
        switch (tag.type) {
        case Tag::Value:
            {
                QString value = context->stringValue(tag.key);
                if (tag.escapeMode == Tag::Escape) {
                    value = escapeHtml(value);
                } else if (tag.escapeMode == Tag::Unescape) {
                    value = unescapeHtml(value);
                }
                output += value;
                lastTagEnd = tag.end;
            }
            break;
        case Tag::SectionStart:
            {
                Tag endTag = findEndTag(_template, tag, endPos);
                if (endTag.type == Tag::Null) {
                    if (m_errorPos == -1) {
                        setError(lit("No matching end tag found for section"), tag.start);
                    }
                } else {
                    int listCount = context->listCount(tag.key);
                    if (listCount > 0) {
                        for (int i=0; i < listCount; i++) {
                            context->push(tag.key, i);
                            output += render(_template, tag.end, endTag.start, context);
                            context->pop();
                        }
                    } else if (context->canEval(tag.key)) {
                        output += context->eval(tag.key, _template.mid(tag.end, endTag.start - tag.end), this);
                    } else if (!context->isFalse(tag.key)) {
                        context->push(tag.key);
                        output += render(_template, tag.end, endTag.start, context);
                        context->pop();
                    }
                    lastTagEnd = endTag.end;
                }
            }
            break;
        case Tag::InvertedSectionStart:
            {
                Tag endTag = findEndTag(_template, tag, endPos);
                if (endTag.type == Tag::Null) {
                    if (m_errorPos == -1) {
                        setError(lit("No matching end tag found for inverted section"), tag.start);
                    }
                } else {
                    if (context->isFalse(tag.key)) {
                        output += render(_template, tag.end, endTag.start, context);
                    }
                    lastTagEnd = endTag.end;
                }
            }
            break;
        case Tag::SectionEnd:
            setError(lit("Unexpected end tag"), tag.start);
            lastTagEnd = tag.end;
            break;
        case Tag::Partial:
            {
                m_partialStack.push(tag.key);

                QString partial = context->partialValue(tag.key);
                output += render(partial, 0, partial.length(), context);
                lastTagEnd = tag.end;

                m_partialStack.pop();
            }
            break;
        case Tag::SetDelimiter:
            lastTagEnd = tag.end;
            break;
        case Tag::Comment:
            lastTagEnd = tag.end;
            break;
        case Tag::Null:
            break;
        }
    }

    return output;
}

void Renderer::setError(const QString& error, int pos)
{
    Q_ASSERT(!error.isEmpty());
    Q_ASSERT(pos >= 0);

    m_error = error;
    m_errorPos = pos;

    if (!m_partialStack.isEmpty())
    {
        m_errorPartial = m_partialStack.top();
    }
}

Tag Renderer::findTag(const QString& content, int pos, int endPos)
{
    int tagStartPos = content.indexOf(m_tagStartMarker, pos);
    if (tagStartPos == -1 || tagStartPos >= endPos) {
        return Tag();
    }

    int tagEndPos = content.indexOf(m_tagEndMarker, tagStartPos + m_tagStartMarker.length());
    if (tagEndPos == -1) {
        return Tag();
    }
    tagEndPos += m_tagEndMarker.length();

    Tag tag;
    tag.type = Tag::Value;
    tag.start = tagStartPos;
    tag.end = tagEndPos;

    pos = tagStartPos + m_tagStartMarker.length();
    endPos = tagEndPos - m_tagEndMarker.length();

    QChar typeChar = content.at(pos);

    if (typeChar == L'#') {
        tag.type = Tag::SectionStart;
        tag.key = readTagName(content, pos+1, endPos);
    } else if (typeChar == L'^') {
        tag.type = Tag::InvertedSectionStart;
        tag.key = readTagName(content, pos+1, endPos);
    } else if (typeChar == L'/') {
        tag.type = Tag::SectionEnd;
        tag.key = readTagName(content, pos+1, endPos);
    } else if (typeChar == L'!') {
        tag.type = Tag::Comment;
    } else if (typeChar == L'>') {
        tag.type = Tag::Partial;
        tag.key = readTagName(content, pos+1, endPos);
    } else if (typeChar == L'=') {
        tag.type = Tag::SetDelimiter;
        readSetDelimiter(content, pos+1, tagEndPos - m_tagEndMarker.length());
    } else {
        if (typeChar == L'&') {
            tag.escapeMode = Tag::Unescape;
            ++pos;
        } else if (typeChar == L'{') {
            tag.escapeMode = Tag::Raw;
            ++pos;
            int endTache = content.indexOf(L'}', pos);
            if (endTache == tag.end - m_tagEndMarker.length()) {
                ++tag.end;
            } else {
                endPos = endTache;
            }
        }
        tag.type = Tag::Value;
        tag.key = readTagName(content, pos, endPos);
    }

    return tag;
}

QString Renderer::readTagName(const QString& content, int pos, int endPos)
{
    QString name;
    name.reserve(endPos - pos);
    while (content.at(pos).isSpace()) {
        ++pos;
    }
    while (!content.at(pos).isSpace() && pos < endPos) {
        name += content.at(pos);
        ++pos;
    }
    return name;
}

void Renderer::readSetDelimiter(const QString& content, int pos, int endPos)
{
    QString startMarker;
    QString endMarker;

    while (!content.at(pos).isSpace() && pos < endPos) {
        if (content.at(pos) == L'=') {
            setError(lit("Custom delimiters may not contain '=' or spaces."), pos);
            return;
        }
        startMarker += content.at(pos);
        ++pos;
    }

    while (content.at(pos).isSpace() && pos < endPos) {
        ++pos;
    }

    while (pos < endPos - 1) {
        if (content.at(pos) == L'=' || content.at(pos).isSpace()) {
            setError(lit("Custom delimiters may not contain '=' or spaces."), pos);
            return;
        }
        endMarker += content.at(pos);
        ++pos;
    }

    m_tagStartMarker = startMarker;
    m_tagEndMarker = endMarker;
}

Tag Renderer::findEndTag(const QString& content, const Tag& startTag, int endPos)
{
    int tagDepth = 1;
    int pos = startTag.end;

    while (true) {
        Tag nextTag = findTag(content, pos, endPos);
        if (nextTag.type == Tag::Null) {
            return nextTag;
        } else if (nextTag.type == Tag::SectionStart || nextTag.type == Tag::InvertedSectionStart) {
            ++tagDepth;
        } else if (nextTag.type == Tag::SectionEnd) {
            --tagDepth;
            if (tagDepth == 0) {
                if (nextTag.key != startTag.key) {
                    setError(lit("Tag start/end key mismatch"), nextTag.start);
                    return Tag();
                }
                return nextTag;
            }
        }
        pos = nextTag.end;
    }

    return Tag();
}

void Renderer::setTagMarkers(const QString& startMarker, const QString& endMarker)
{
    m_defaultTagStartMarker = startMarker;
    m_defaultTagEndMarker = endMarker;
}

#endif // ENABLE_SENDMAIL
