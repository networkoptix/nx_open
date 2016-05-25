#ifndef CONTEXT_SETTINGS_H
#define CONTEXT_SETTINGS_H

#include <QtCore/QObject>

class QnContextSettingsPrivate;
class QnContextSettings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString sessionId READ sessionId WRITE setSessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(bool showOfflineCameras READ showOfflineCameras WRITE setShowOfflineCameras NOTIFY showOfflineCamerasChanged)
    Q_PROPERTY(QStringList hiddenCameras READ hiddenCameras WRITE setHiddenCameras NOTIFY hiddenCamerasChanged)
    Q_PROPERTY(bool hiddenCamerasCollapsed READ hiddenCamerasCollapsed WRITE setHiddenCamerasCollapsed NOTIFY hiddenCamerasCollapsedChanged)

public:
    explicit QnContextSettings(QObject *parent = 0);
    ~QnContextSettings();

    bool showOfflineCameras() const;
    void setShowOfflineCameras(bool showOfflineCameras);

    QString sessionId() const;
    void setSessionId(const QString &sessionId);

    QStringList hiddenCameras() const;
    void setHiddenCameras(const QStringList &hiddenCameras);

    bool hiddenCamerasCollapsed() const;
    void setHiddenCamerasCollapsed(bool collapsed);

signals:
    void sessionIdChanged();
    void showOfflineCamerasChanged();
    void hiddenCamerasChanged();
    void hiddenCamerasCollapsedChanged();

private:

private:
    Q_DECLARE_PRIVATE(QnContextSettings)
    QScopedPointer<QnContextSettingsPrivate> d_ptr;
};

#endif // CONTEXT_SETTINGS_H
