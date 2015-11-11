#ifndef MEDIA_RESOURCE_HELPER_H
#define MEDIA_RESOURCE_HELPER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <utils/common/connective.h>

class QnMediaResourceHelper : public Connective<QObject> {
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QUrl mediaUrl READ mediaUrl NOTIFY mediaUrlChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(QStringList resolutions READ resolutions NOTIFY resolutionsChanged)
    Q_PROPERTY(QString resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QSize screenSize READ screenSize WRITE setScreenSize NOTIFY screenSizeChanged)
    Q_PROPERTY(Protocol protocol READ protocol NOTIFY protocolChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(qreal rotatedAspectRatio READ rotatedAspectRatio NOTIFY rotatedAspectRatioChanged)
    Q_PROPERTY(int rotation READ rotation NOTIFY rotationChanged)

    Q_ENUMS(Protocol)

    typedef Connective<QObject> base_type;

public:
    enum Protocol {
        Webm,
        Rtsp,
        Hls,
        Mjpeg
    };

    explicit QnMediaResourceHelper(QObject *parent = 0);

    QString resourceId() const;
    void setResourceId(const QString &id);

    QUrl mediaUrl() const;

    QString resourceName() const;

    Q_INVOKABLE void setPosition(qint64 position);

    QStringList resolutions() const;
    QString resolution() const;
    void setResolution(const QString &resolution);

    QSize screenSize() const;
    void setScreenSize(const QSize &size);

    QString optimalResolution() const;

    Protocol protocol() const;

    qreal aspectRatio() const;
    qreal sensorAspectRatio() const;
    qreal rotatedAspectRatio() const;

    int rotation() const;

signals:
    void resourceIdChanged();
    void mediaUrlChanged();
    void resourceNameChanged();
    void resolutionsChanged();
    void resolutionChanged();
    void screenSizeChanged();
    void protocolChanged();
    void aspectRatioChanged();
    void rotatedAspectRatioChanged();
    void rotationChanged();

private:
    void updateMediaStreams();
    void at_resource_parentIdChanged(const QnResourcePtr &resource);

private:
    void setStardardResolutions();
    void setUrl(const QUrl &url);
    void updateUrl();
    int nativeStreamIndex(const QString &resolution) const;
    QString resolutionString(int resolution) const;
    QString currentResolutionString() const;

private:
    QnVirtualCameraResourcePtr m_camera;
    QUrl m_url;
    qint64 m_position;
    int m_resolution;
    QList<int> m_standardResolutions;
    QSize m_screenSize;
    int m_nativeStreamIndex;
    QMap<int, QString> m_nativeResolutions;
    bool m_transcodingSupported;
    Protocol m_transcodingProtocol;
    Protocol m_nativeProtocol;
};

#endif // MEDIA_RESOURCE_HELPER_H
