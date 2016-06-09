#ifndef QN_UBJSON_FWD_H
#define QN_UBJSON_FWD_H

class QByteArray;

template<class Input>
class QnUbjsonReader;

template<class Output>
class QnUbjsonWriter;


#define QN_FUSION_DECLARE_FUNCTIONS_ubjson(TYPE, ... /* PREFIX */)              \
__VA_ARGS__ void serialize(const TYPE &value, QnUbjsonWriter<QByteArray> *stream); \
__VA_ARGS__ bool deserialize(QnUbjsonReader<QByteArray> *stream, TYPE *target);

#endif // QN_UBJSON_FWD_H
