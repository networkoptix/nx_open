#ifndef QN_SERIALIZATION_BINARY_FWD_H
#define QN_SERIALIZATION_BINARY_FWD_H

class QByteArray;

/**
 * Stream providing read operation.
 */
template<class Input>
class QnInputBinaryStream;

/**
 * Stream providing write operation
 */
template<class Output>
class QnOutputBinaryStream;


#define QN_FUSION_DECLARE_FUNCTIONS_binary(TYPE, ... /* PREFIX */)              \
__VA_ARGS__ void serialize(const TYPE &value, QnOutputBinaryStream<QByteArray> *stream); \
__VA_ARGS__ bool deserialize(QnInputBinaryStream<QByteArray> *stream, TYPE *target);

#endif // QN_SERIALIZATION_BINARY_FWD_H
