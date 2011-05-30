#ifndef uniclient_filetypesupport_h
#define uniclient_filetypesupport_h

class FileTypeSupport
{
public:
    FileTypeSupport();

    bool isFileSupported(const QString& filename) const;

    const QStringList& imagesFilter() const;
    const QStringList& moviesFilter() const;
private:
    QStringList m_imageFileExtensions;
    QStringList m_movieFileExtensions;

    QStringList m_imageFileFilter;
    QStringList m_movieFileFilter;
};

#endif // uniclient_filetypesupport_h
