// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QEvent>

class NX_VMS_COMMON_API FileTypeSupport
{
public:
    static bool isFileSupported(const QString& filename);

    static bool isMovieFileExt(const QString& filename);
    static bool isImageFileExt(const QString& filename);
    static bool isValidLayoutFile(const QString& filename);
};
