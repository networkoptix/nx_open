#pragma once

#include <utils/common/connective.h>

class QnMediaResourceHelperPrivate;
class QnMediaResourceHelper : public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(qreal rotatedAspectRatio READ rotatedAspectRatio NOTIFY rotatedAspectRatioChanged)
    Q_PROPERTY(int rotation READ rotation NOTIFY rotationChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = Connective<QObject>;

public:
    explicit QnMediaResourceHelper(QObject* parent = nullptr);
    ~QnMediaResourceHelper();

    QString resourceId() const;
    void setResourceId(const QString& id);

    Qn::ResourceStatus resourceStatus() const;
    QString resourceName() const;
    qreal aspectRatio() const;
    qreal sensorAspectRatio() const;
    qreal rotatedAspectRatio() const;
    int rotation() const;

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void resourceNameChanged();
    void aspectRatioChanged();
    void rotatedAspectRatioChanged();
    void rotationChanged();

private:
    QScopedPointer<QnMediaResourceHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMediaResourceHelper)
};
