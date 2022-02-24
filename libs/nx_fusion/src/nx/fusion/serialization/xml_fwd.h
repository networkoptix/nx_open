// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_XML_FWD_H
#define QN_SERIALIZATION_XML_FWD_H

class QXmlStreamWriter;

#define QN_FUSION_DECLARE_FUNCTIONS_xml(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ void serialize(const TYPE &value, QXmlStreamWriter *stream);

#endif // QN_SERIALIZATION_XML_FWD_H
