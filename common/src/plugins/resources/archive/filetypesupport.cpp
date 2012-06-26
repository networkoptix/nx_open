#include "filetypesupport.h"

#include "filetypes.h"
#include "utils/common/util.h"


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
    return filename.toLower().endsWith(".nov");
}

bool FileTypeSupport::isFileSupported(const QString &filename)
{
    return isImageFileExt(filename) || isMovieFileExt(filename) || isLayoutFileExt(filename);
}
