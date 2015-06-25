#ifndef CONTEXT_SETTINGS_H
#define CONTEXT_SETTINGS_H

#include <QtCore/QObject>

class QnContextSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool showOfflineCameras READ showOfflineCameras WRITE setShowOfflineCameras NOTIFY showOfflineCamerasChanged)
public:
    explicit QnContextSettings(QObject *parent = 0);

    bool showOfflineCameras() const;
    void setShowOfflineCameras(bool showOfflineCameras);

signals:
    void showOfflineCamerasChanged();

private:
    void at_valueChanged(int valueId);
};

#endif // CONTEXT_SETTINGS_H
