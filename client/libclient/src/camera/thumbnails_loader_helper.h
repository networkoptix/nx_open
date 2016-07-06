#ifndef QN_THUMBNAIL_LOADER_HELPER_H
#define QN_THUMBNAIL_LOADER_HELPER_H

#include <QtCore/QObject>

#include "thumbnails_loader.h"

class QnThumbnailsLoaderHelper: public QObject {
    Q_OBJECT;
public:
    QnThumbnailsLoaderHelper(QnThumbnailsLoader *loader): m_loader(loader) {}

signals:
    void thumbnailLoaded(const QnThumbnail &thumbnail);

public slots:
    void process() {
        m_loader->process();
    }

private:
    friend class QnThumbnailsLoader;

    QnThumbnailsLoader *m_loader;
};


#endif // QN_THUMBNAIL_LOADER_HELPER_H
