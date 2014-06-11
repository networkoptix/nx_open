#ifndef QN_UBJ_FWD_H
#define QN_UBJ_FWD_H

class QByteArray;

template<class Input>
class QnUbjReader;

template<class Output>
class QnUbjWriter;


#define QN_FUSION_DECLARE_FUNCTIONS_ubj(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ void serialize(const TYPE &value, QnUbjWriter<QByteArray> *stream); \
__VA_ARGS__ bool deserialize(QnUbjReader<QByteArray> *stream, TYPE *target);

#endif // QN_UBJ_FWD_H
