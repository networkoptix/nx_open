#ifndef uniclient_filetypesupport_h
#define uniclient_filetypesupport_h

class QN_EXPORT FileTypeSupport
{
public:
    static bool isFileSupported(const QString &filename);

    static bool isMovieFileExt(const QString &filename);
    static bool isImageFileExt(const QString &filename);

};

#endif // uniclient_filetypesupport_h
