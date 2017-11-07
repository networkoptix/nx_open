#ifndef uniclient_filetypesupport_h
#define uniclient_filetypesupport_h

#include <QtCore/QString>

class QN_EXPORT FileTypeSupport
{
public:
    static const quint64 NOV_EXE_MAGIC = 0x73a0b934820d4055ull;

    static bool isFileSupported(const QString &filename);

    static bool isMovieFileExt(const QString &filename);
    static bool isImageFileExt(const QString &filename);
    static bool isLayoutFileExt(const QString &filename);
};

#endif // uniclient_filetypesupport_h
