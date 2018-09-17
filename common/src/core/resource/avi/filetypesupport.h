#ifndef uniclient_filetypesupport_h
#define uniclient_filetypesupport_h

#include <QtCore/QString>

class FileTypeSupport
{
public:

    static bool isFileSupported(const QString &filename);

    static bool isMovieFileExt(const QString &filename);
    static bool isImageFileExt(const QString &filename);
    static bool isLayoutFileExt(const QString &filename);
};

#endif // uniclient_filetypesupport_h
