#ifndef QN_SERIALIZATION_XML_FWD_H
#define QN_SERIALIZATION_XML_FWD_H

class QXmlStreamWriter;

#define QN_FUSION_DECLARE_FUNCTIONS_xml(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ void serialize(const TYPE &value, QXmlStreamWriter *stream);

#endif // QN_SERIALIZATION_XML_FWD_H
