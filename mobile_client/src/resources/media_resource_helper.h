#ifndef MEDIA_RESOURCE_HELPER_H
#define MEDIA_RESOURCE_HELPER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

class QnMediaResourceHelper : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QUrl mediaUrl READ mediaUrl NOTIFY mediaUrlChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(QStringList resolutions READ resolutions NOTIFY resolutionsChanged)
    Q_PROPERTY(QString stream READ stream WRITE setStream NOTIFY streamChanged)
    Q_PROPERTY(QSize screenSize READ screenSize WRITE setScreenSize NOTIFY screenSizeChanged)

    Q_ENUMS(Protocol)

public:
    enum Protocol {
        Http,
        Rtsp,
        UnknownProtocol
    };

    explicit QnMediaResourceHelper(QObject *parent = 0);

    QString resourceId() const;
    void setResourceId(const QString &id);

    QUrl mediaUrl() const;

    QString resourceName() const;

    Q_INVOKABLE void setDateTime(const QDateTime &dateTime);
    Q_INVOKABLE void setLive();

    QStringList resolutions() const;
    QString stream() const;
    void setStream(const QString &stream);

    QSize screenSize() const;
    void setScreenSize(const QSize &size);

    QString optimalResolution() const;

signals:
    void resourceIdChanged();
    void mediaUrlChanged();
    void resourceNameChanged();
    void resolutionsChanged();
    void streamChanged();
    void screenSizeChanged();

private:
    void at_resourcePropertyChanged(const QnResourcePtr &resource, const QString &key);

private:
    void setStardardResolutions();
    void setUrl(const QUrl &url);
    void updateUrl();

private:
    QnResourcePtr m_resource;
    QUrl m_url;
    QDateTime m_dateTime;
    Protocol m_protocol;
    CameraMediaStreams m_supportedStreams;
    QString m_resolution;
    int m_standardResolution;
    QList<int> m_standardResolutions;
    QSize m_screenSize;
    qreal m_aspectRatio;
    int m_nativeStreamIndex;
};

#endif // MEDIA_RESOURCE_HELPER_H
