#pragma once

#include <core/ptz/media_dewarping_params.h>
#include <utils/common/connective.h>

class QnMediaResourceHelperPrivate;
class QnMediaResourceHelper : public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(QString serverName READ serverName NOTIFY serverNameChanged)
    Q_PROPERTY(qreal customAspectRatio READ customAspectRatio NOTIFY customAspectRatioChanged)
    Q_PROPERTY(int customRotation READ customRotation NOTIFY customRotationChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY videoLayoutChanged)
    Q_PROPERTY(QSize layoutSize READ layoutSize NOTIFY videoLayoutChanged)
    Q_PROPERTY(QnMediaDewarpingParams fisheye READ fisheye NOTIFY fisheyeChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = Connective<QObject>;

public:
    explicit QnMediaResourceHelper(QObject* parent = nullptr);
    ~QnMediaResourceHelper();

    QString resourceId() const;
    void setResourceId(const QString& id);

    Qn::ResourceStatus resourceStatus() const;
    QString resourceName() const;
    QString serverName() const;
    qreal customAspectRatio() const;
    int customRotation() const;
    int channelCount() const;
    QSize layoutSize() const;
    QnMediaDewarpingParams fisheye() const;
    Q_INVOKABLE QPoint channelPosition(int channel) const;

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void resourceNameChanged();
    void serverNameChanged();
    void customAspectRatioChanged();
    void customRotationChanged();
    void videoLayoutChanged();
    void fisheyeChanged();

private:
    QScopedPointer<QnMediaResourceHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMediaResourceHelper)
};
