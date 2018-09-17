#include "filetypesupport.h"

#include <QtCore/QFile>
extern "C"
{
    #include <libavutil/avutil.h>
}

#include <nx/core/layout/layout_file_info.h>

#include "utils/common/util.h"

static const char* VIDEO_FILETYPES[] = {
    "3g2", "3gp", "3gp2", "3gpp", "amv", "asf", "avi", "divx", "dv",
    "flv", "gxf", "m1v", "m2t", "m2v", "m2ts", "m4v", "mkv", "mov",
    "mp2", "mp2v", "mp4", "mp4v", "mpa", "mpe", "mpeg", "mpeg1",
    "mpeg2", "mpeg4", "mpg", "mpv2", "mts", "mxf", "nsv", "nuv",
    "ogg", "ogm", "ogx", "ogv", "rec", "rm", "rmvb", "tod", "ts",
    "tts", "vob", "vro", "webm", "wmv"};

static const char* IMAGE_FILETYPES[] = {"jpg", "jpeg", "png", "bmp", "gif", "tif", "tiff"};

bool FileTypeSupport::isMovieFileExt(const QString &filename)
{
    const QString lowerFilename = filename.toLower();

    for (unsigned i = 0; i < arraysize(VIDEO_FILETYPES); i++)
    {
        if (lowerFilename.endsWith(QString::fromLatin1(VIDEO_FILETYPES[i])))
            return true;

    }
    return false;
}

bool FileTypeSupport::isImageFileExt(const QString &filename)
{
    const QString lowerFilename = filename.toLower();

    for (unsigned i = 0; i < arraysize(IMAGE_FILETYPES); i++)
    {
        if (lowerFilename.endsWith(QString::fromLatin1(IMAGE_FILETYPES[i])))
            return true;

    }
    return false;
}

bool FileTypeSupport::isLayoutFileExt(const QString &filename)
{
    return nx::core::layout::identifyFile(filename).isValid;
}

bool FileTypeSupport::isFileSupported(const QString &filename)
{
    return isImageFileExt(filename) || isMovieFileExt(filename) || isLayoutFileExt(filename);
}
