#pragma once

#include <QtCore/QObject>
#include <nx/utils/uuid.h>

class QnMobileClientUiControllerPrivate;
class QnMobileClientUiController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)

public:
    QnMobileClientUiController(QObject* parent = nullptr);
    ~QnMobileClientUiController();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

public slots:
    void disconnectFromSystem();
    void openResourcesScreen();
    void openVideoScreen(const QnUuid& cameraId);

signals:
    void disconnectRequested();
    void layoutIdChanged();
    void resourcesScreenRequested();
    void videoScreenRequested(const QString& cameraId);

private:
    QScopedPointer<QnMobileClientUiControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMobileClientUiController)
};
