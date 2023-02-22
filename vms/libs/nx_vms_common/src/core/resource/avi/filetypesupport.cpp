// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filetypesupport.h"

#include <QSet>

#include <nx/core/layout/layout_file_info.h>

bool FileTypeSupport::isMovieFileExt(const QString &filename)
{
    static const QSet<QString> kVideoExtensions{
        "3g2", "3gp", "3gp2", "3gpp", "amv", "asf", "avi", "divx", "dv", "flv", "gxf", "m1v",
        "m2t", "m2v", "m2ts", "m4v", "mkv", "mov", "mp2", "mp2v", "mp4", "mp4v", "mpa", "mpe",
        "mpeg", "mpeg1", "mpeg2", "mpeg4", "mpg", "mpv2", "mts", "mxf", "nsv", "nuv", "ogg", "ogm",
        "ogx", "ogv", "rec", "rm", "rmvb", "tod", "ts", "tts", "vob", "vro", "webm", "wmv",
        "mpegts"
    };

    const auto i = filename.lastIndexOf('.');
    if (i == -1)
        return false;

    const QString lowerExt = filename.sliced(i + 1).toLower();

    return kVideoExtensions.contains(lowerExt);
}

bool FileTypeSupport::isImageFileExt(const QString &filename)
{
    static const QSet<QString> kImageExtensions{
        "jpg", "jpeg", "png", "bmp", "gif", "tif", "tiff"
    };

    const auto i = filename.lastIndexOf('.');
    if (i == -1)
        return false;

    const QString lowerExt = filename.sliced(i + 1).toLower();

    return kImageExtensions.contains(lowerExt);
}

bool FileTypeSupport::isValidLayoutFile(const QString &filename)
{
    return nx::core::layout::identifyFile(filename).isValid;
}

bool FileTypeSupport::isFileSupported(const QString &filename)
{
    return isImageFileExt(filename) || isMovieFileExt(filename) || isValidLayoutFile(filename);
}
