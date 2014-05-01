#ifndef QN_BYTE_STRING_H
#define QN_BYTE_STRING_H

#include <QtCore/QByteArray>

/**
 * A byte array that is to be treated by serialization / deserialization 
 * routines as a latin1 string.
 */
class QnLatin1Array: public QByteArray {
public:
    QnLatin1Array() {}
    
    QnLatin1Array(const QByteArray &other): QByteArray(other) {}
};

#endif // QN_BYTE_STRING_H
