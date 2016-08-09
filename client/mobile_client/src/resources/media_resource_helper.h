#pragma once

#include <utils/common/connective.h>

class QnMediaResourceHelperPrivate;
class QnMediaResourceHelper : public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(qreal customAspectRatio READ customAspectRatio NOTIFY customAspectRatioChanged)
    Q_PROPERTY(int customRotation READ customRotation NOTIFY customRotationChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = Connective<QObject>;

public:
    explicit QnMediaResourceHelper(QObject* parent = nullptr);
    ~QnMediaResourceHelper();

    QString resourceId() const;
    void setResourceId(const QString& id);

    Qn::ResourceStatus resourceStatus() const;
    QString resourceName() const;
    qreal customAspectRatio() const;
    int customRotation() const;

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void resourceNameChanged();
    void customAspectRatioChanged();
    void customRotationChanged();

private:
    QScopedPointer<QnMediaResourceHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMediaResourceHelper)
};
