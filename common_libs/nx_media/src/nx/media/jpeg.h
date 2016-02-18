#pragma once

#include <QtGui/QImage>

QImage decompressJpegImage(const char *data, size_t size);
QImage decompressJpegImage(const QByteArray &data);
