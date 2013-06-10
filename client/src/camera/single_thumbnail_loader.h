#ifndef SINGLE_THUMBNAIL_LOADER_H
#define SINGLE_THUMBNAIL_LOADER_H

#include <QtCore/QObject>

#include <api/media_server_connection.h>
#include <core/resource/network_resource.h>

class QnSingleThumbnailLoader : public QObject
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param connection                Video server connection to use.
     * \param resource                  Network resource representing the camera to work with.
     * \param parent                    Parent object.
     */
    explicit QnSingleThumbnailLoader(const QnMediaServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent = NULL);

    /**
     * Creates a new thumbnail loader for the given camera resource. Returns NULL
     * pointer in case loader cannot be created.
     *
     * \param resource                  Camera resource to create time period loader for.
     * \param parent                    Parent object for the loader to create.
     * \returns                         Newly created thumbnail loader.
     */
    static QnSingleThumbnailLoader *newInstance(QnResourcePtr resource, QObject *parent = NULL);

    void load(qint64 usecSinceEpoch, const QSize& size);
signals:
    /**
     * This signal is emitted whenever thumbnail was successfully loaded.
     *
     * \param image                     Loaded thumbnail.
     */
    void success(const QImage& image);

    /**
     * This signal is emitted whenever the loader was unable to load thumbnail.
     *
     * \param status                    Error code.
     */
    void failed(int status);

    void finished();
private slots:
    void at_replyReceived(int status, const QImage& image, int requstHandle);

private:
    int sendRequest(qint64 usecSinceEpoch, const QSize& size);

private:
    /** Resource that this loader gets thumbnail for. */
    QnResourcePtr m_resource;

    /** Video server connection that this loader uses. */
    QnMediaServerConnectionPtr m_connection;
};

#endif // SINGLE_THUMBNAIL_LOADER_H
