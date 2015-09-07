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
    Q_PROPERTY(QString resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QSize screenSize READ screenSize WRITE setScreenSize NOTIFY screenSizeChanged)
    Q_PROPERTY(QString protocol READ protocol NOTIFY protocolChanged)

public:
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

    QString protocol() const;

signals:
    void resourceIdChanged();
    void mediaUrlChanged();
    void resourceNameChanged();
    void resolutionsChanged();
    void resolutionChanged();
    void screenSizeChanged();
    void protocolChanged();

private:
    void at_resourcePropertyChanged(const QnResourcePtr &resource, const QString &key);
    void at_resource_parentIdChanged(const QnResourcePtr &resource);

private:
    void setStardardResolutions();
    void setUrl(const QUrl &url);
    void updateUrl();
    int nativeStreamIndex(const QString &resolution) const;

private:
    QnResourcePtr m_resource;
    QUrl m_url;
    qint64 m_position;
    QString m_resolution;
    QList<int> m_standardResolutions;
    QSize m_screenSize;
    int m_nativeStreamIndex;
    QMap<int, QString> m_nativeResolutions;
    bool m_transcodingSupported;
};

#endif // MEDIA_RESOURCE_HELPER_H
