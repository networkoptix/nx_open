// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QImage>

QImage decompressJpegImage(const char *data, size_t size);
QImage decompressJpegImage(const QByteArray &data);
