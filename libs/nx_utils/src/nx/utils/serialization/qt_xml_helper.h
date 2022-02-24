// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QXmlStreamReader>

namespace nx::utils {

/**
 * Runs given QXmlStreamReader and reports XML parsing events to the handler that has an API
 * similar to QXmlDefaultHandler.
 * This is a helper function for switching from deprecated QtXml (QXmlDefaultHandler) to
 * QXmlStreamReader.
 */
template<typename Handler>
bool parseXml(
    QXmlStreamReader& reader,
    Handler& handler)
{
    while (!reader.atEnd())
    {
        bool result = true;
        reader.readNext();
        switch (reader.tokenType())
        {
            case QXmlStreamReader::TokenType::StartDocument:
                result = handler.startDocument();
                break;

            case QXmlStreamReader::TokenType::EndDocument:
                result = handler.endDocument();
                break;

            case QXmlStreamReader::TokenType::StartElement:
                result = handler.startElement(
                    reader.namespaceUri(),
                    reader.name(),
                    reader.attributes());
                break;

            case QXmlStreamReader::TokenType::EndElement:
                result = handler.endElement(
                    reader.namespaceUri(),
                    reader.name());
                break;

            case QXmlStreamReader::TokenType::Characters:
                result = handler.characters(reader.text());
                break;

            case QXmlStreamReader::Invalid:
                result = false;
                break;

            default:
                break;
        }

        if (!result)
            return false;
    }

    return true;
}

} // namespace nx::utils
