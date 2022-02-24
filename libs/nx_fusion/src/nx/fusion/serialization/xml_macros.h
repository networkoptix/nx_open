// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "xml.h"

#include <nx/fusion/fusion/fusion.h>

namespace QnXmlDetail {

class SerializationVisitor
{
public:
    SerializationVisitor(QXmlStreamWriter* stream): m_stream(stream) {}

    template<class T, class Access>
    bool operator()(const T& value, const Access& access)
    {
        using namespace QnFusion;

        m_stream->writeStartElement(access(name));
        QnXmlDetail::serialize(invoke(access(getter), value), m_stream);
        m_stream->writeEndElement();
        return true;
    }

private:
    QXmlStreamWriter* m_stream;
};

} // namespace QnXmlDetail

#define QN_FUSION_DEFINE_FUNCTIONS_xml(TYPE, /*PREFIX*/ ...) \
    __VA_ARGS__ void serialize(const TYPE& value, QXmlStreamWriter* stream) \
    { \
        QnXmlDetail::SerializationVisitor visitor(stream); \
        QnFusion::visit_members(value, visitor); \
    }

#define QN_FUSION_DEFINE_FUNCTIONS_xml_lexical(TYPE, /*PREFIX*/ ...) \
    __VA_ARGS__ void serialize(const TYPE& value, QXmlStreamWriter* stream) \
    { \
        /* NOTE: writeCharacters() adds chars prohibited in XML, including '\0', as is. */ \
        stream->writeCharacters(QnXml::replaceProhibitedChars(QnLexical::serialized(value))); \
    }
