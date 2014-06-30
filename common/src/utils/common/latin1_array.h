#ifndef QN_LATIN1_ARRAY_H
#define QN_LATIN1_ARRAY_H

#include <QtCore/QByteArray>


/**
 * A byte array that is to be treated by serialization / deserialization 
 * routines as a latin1 string.
 */
class QnLatin1Array: public QByteArray {
public:
    QnLatin1Array() {}
    
    QnLatin1Array(const QByteArray &other): QByteArray(other) {}
    QnLatin1Array(const char* data): QByteArray(data) {}
};


#endif // QN_LATIN1_ARRAY_H
