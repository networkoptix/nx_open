#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <utils/common/connective.h>

class QnCameraChunkProvider;

class QnMediaResourceHelper : public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QUrl mediaUrl READ mediaUrl NOTIFY mediaUrlChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QStringList resolutions READ resolutions NOTIFY resolutionsChanged)
    Q_PROPERTY(QString resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QSize screenSize READ screenSize WRITE setScreenSize NOTIFY screenSizeChanged)
    Q_PROPERTY(Protocol protocol READ protocol NOTIFY protocolChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(qreal rotatedAspectRatio READ rotatedAspectRatio NOTIFY rotatedAspectRatioChanged)
    Q_PROPERTY(int rotation READ rotation NOTIFY rotationChanged)
    Q_PROPERTY(qint64 finalTimestamp READ finalTimestamp NOTIFY finalTimestampChanged)
    Q_PROPERTY(QnCameraChunkProvider* chunkProvider READ chunkProvider NOTIFY chunkProviderChanged)

    Q_ENUMS(Protocol)
    Q_ENUMS(Qn::ResourceStatus)

    typedef Connective<QObject> base_type;

public:
    enum Protocol
    {
        Webm,
        Rtsp,
        Hls,
        Mjpeg
    };

    explicit QnMediaResourceHelper(QObject *parent = 0);

    QString resourceId() const;
    void setResourceId(const QString &id);

    Qn::ResourceStatus resourceStatus() const;

    QUrl mediaUrl() const;

    QString resourceName() const;

    QnCameraChunkProvider *chunkProvider() const;

    qint64 position() const;
    void setPosition(qint64 position);

    QStringList resolutions() const;
    QString resolution() const;
    void setResolution(const QString &resolution);

    QSize screenSize() const;
    void setScreenSize(const QSize &size);

    Protocol protocol() const;

    qreal aspectRatio() const;
    qreal sensorAspectRatio() const;
    qreal rotatedAspectRatio() const;

    int rotation() const;

    qint64 finalTimestamp() const;

    Q_INVOKABLE void updateUrl();

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void mediaUrlChanged();
    void resourceNameChanged();
    void chunkProviderChanged();
    void positionChanged();
    void resolutionsChanged();
    void resolutionChanged();
    void screenSizeChanged();
    void protocolChanged();
    void aspectRatioChanged();
    void rotatedAspectRatioChanged();
    void rotationChanged();
    void finalTimestampChanged();

private:
    void updateMediaStreams();
    void updateCurrentStream();
    void at_resource_parentIdChanged(const QnResourcePtr &resource);
    void updateStardardResolutions();
    void setUrl(const QUrl &url);
    int nativeStreamIndex(const QString &resolution) const;
    QString resolutionString(int resolution) const;
    QString currentResolutionString() const;
    int optimalResolution() const;
    int maximumResolution() const;
    void updateFinalTimestamp();
    void setFinalTimestamp(qint64 finalTimestamp);
    QnMediaServerResourcePtr serverAtCurrentPosition() const;

private:
    QnVirtualCameraResourcePtr m_camera;
    QUrl m_url;
    qint64 m_position;
    qint64 m_finalTimestamp;
    int m_resolution;
    QList<int> m_standardResolutions;
    QSize m_screenSize;
    int m_nativeStreamIndex;
    QMap<int, QString> m_nativeResolutions;
    bool m_useTranscoding;
    Protocol m_transcodingProtocol;
    Protocol m_nativeProtocol;
    int m_maxTextureSize;
    int m_maxNativeResolution;
    QnCameraChunkProvider *m_chunkProvider;
};
