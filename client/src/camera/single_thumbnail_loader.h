#ifndef SINGLE_THUMBNAIL_LOADER_H
#define SINGLE_THUMBNAIL_LOADER_H

#include <QtCore/QObject>

#include <api/media_server_connection.h>
#include <core/resource/resource_fwd.h>
#include <utils/image_provider.h>

class QnSingleThumbnailLoader : public QnImageProvider {
    Q_OBJECT

    typedef QnImageProvider base_type;

public:
    enum ThumbnailFormat {
        PngFormat,
        JpgFormat
    };

    /**
     * Creates a new thumbnail loader for the given camera resource. Returns NULL
     * pointer in case loader cannot be created.
     *
     * \param resource                  Camera resource to create time period loader for.
     * \param parent                    Parent object for the loader to create.
     * \returns                         Newly created thumbnail loader.
     */
    static QnSingleThumbnailLoader *newInstance(QnResourcePtr resource,
                                                qint64 microSecSinceEpoch,
                                                const QSize &size = QSize(),
                                                ThumbnailFormat format = PngFormat,
                                                QObject *parent = NULL);


    /**
     * Constructor.
     *
     * \param connection                Video server connection to use.
     * \param resource                  Network resource representing the camera to work with.
     * \param parent                    Parent object.
     */
    explicit QnSingleThumbnailLoader(const QnMediaServerConnectionPtr &connection,
                                     QnNetworkResourcePtr resource,
                                     qint64 microSecSinceEpoch,
                                     const QSize &size,
                                     ThumbnailFormat format,
                                     QObject *parent = NULL);

    virtual QImage image() const override;
protected:
    virtual void doLoadAsync() override;

    QString formatToString(ThumbnailFormat format);
private slots:
    void at_replyReceived(int status, const QImage& image, int requstHandle);

private:
    /** Resource that this loader gets thumbnail for. */
    QnResourcePtr m_resource;

    /** Video server connection that this loader uses. */
    QnMediaServerConnectionPtr m_connection;

    QImage m_image;

    /** Time in microseconds since epoch */
    qint64 m_microSecSinceEpoch;
    QSize m_size;

    ThumbnailFormat m_format;
};

#endif // SINGLE_THUMBNAIL_LOADER_H
