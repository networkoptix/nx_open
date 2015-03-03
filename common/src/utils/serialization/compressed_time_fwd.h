#ifndef QN_COMPRESSED_TIME_FWD_H
#define QN_COMPRESSED_TIME_FWD_H

class QByteArray;

template<class Input>
class QnCompressedTimeReader;

template<class Output>
class QnCompressedTimeWriter;


#define QN_FUSION_DECLARE_FUNCTIONS_compressed_time(TYPE, ... /* PREFIX */)              \
__VA_ARGS__ void serialize(const TYPE &value, QnCompressedTimeWriter<QByteArray> *stream); \
__VA_ARGS__ bool deserialize(QnCompressedTimeReader<QByteArray> *stream, TYPE *target);

#endif // QN_COMPRESSED_TIME_FWD_H
