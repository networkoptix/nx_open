#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace core {
namespace resource {

class LayoutAccessor: public Connective<QObject>
{
    Q_OBJECT
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)

    using base_type = Connective<QObject>;

public:
    LayoutAccessor(QObject* parent = nullptr);
    ~LayoutAccessor();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QString name() const;

signals:
    void layoutAboutToBeChanged();
    void layoutChanged();
    void nameChanged();

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace resource
} // namespace core
} // namespace client
} // namespace nx
