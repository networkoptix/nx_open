#ifndef GZIP_COMPRESSOR_H
#define GZIP_COMPRESSOR_H

#include <QObject>

class GZipCompressor
{
public:
    static QByteArray compressData(const QByteArray& data);
};

#endif // GZIP_COMPRESSOR_H
