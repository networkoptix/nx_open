#ifndef QN_SERIALIZATION_DATA_STREAM_FWD_H
#define QN_SERIALIZATION_DATA_STREAM_FWD_H

class QDataStream;

#define QN_FUSION_DECLARE_FUNCTIONS_datastream(TYPE, ... /* PREFIX */)          \
__VA_ARGS__ QDataStream &operator<<(QDataStream &stream, const TYPE &value);    \
__VA_ARGS__ QDataStream &operator>>(QDataStream &stream, TYPE &value);

#endif // QN_SERIALIZATION_DATA_STREAM_FWD_H
