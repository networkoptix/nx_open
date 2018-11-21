#include "filetypesupport.h"

#include <vector>

#include <nx/core/layout/layout_file_info.h>

bool FileTypeSupport::isMovieFileExt(const QString &filename)
{
    static const std::vector<QString> kVideoExtensions{
        "3g2", "3gp", "3gp2", "3gpp", "amv", "asf", "avi", "divx", "dv", "flv", "gxf", "m1v",
        "m2t", "m2v", "m2ts", "m4v", "mkv", "mov", "mp2", "mp2v", "mp4", "mp4v", "mpa", "mpe",
        "mpeg", "mpeg1", "mpeg2", "mpeg4", "mpg", "mpv2", "mts", "mxf", "nsv", "nuv", "ogg", "ogm",
        "ogx", "ogv", "rec", "rm", "rmvb", "tod", "ts", "tts", "vob", "vro", "webm", "wmv"
    };

    const QString lowerFilename = filename.toLower();

    for (const auto& extension: kVideoExtensions)
    {
        if (lowerFilename.endsWith('.' + extension))
            return true;
    }
    return false;
}

bool FileTypeSupport::isImageFileExt(const QString &filename)
{
    static const std::vector<QString> kImageExtensions{
        "jpg", "jpeg", "png", "bmp", "gif", "tif", "tiff"
    };

    const QString lowerFilename = filename.toLower();

    for (const auto& extension: kImageExtensions)
    {
        if (lowerFilename.endsWith('.' + extension))
            return true;
    }
    return false;
}

bool FileTypeSupport::isValidLayoutFile(const QString &filename)
{
    return nx::core::layout::identifyFile(filename).isValid;
}

bool FileTypeSupport::isFileSupported(const QString &filename)
{
    return isImageFileExt(filename) || isMovieFileExt(filename) || isValidLayoutFile(filename);
}
