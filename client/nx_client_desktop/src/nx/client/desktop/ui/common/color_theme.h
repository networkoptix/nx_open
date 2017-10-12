#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>

#include <nx/utils/singleton.h>

namespace nx {
namespace client {
namespace desktop {

class ColorTheme: public QObject, public Singleton<ColorTheme>
{
    Q_OBJECT

    Q_PROPERTY(QVariantMap colors READ colors CONSTANT)

public:
    explicit ColorTheme(QObject* parent = nullptr);
    virtual ~ColorTheme() override;

    QVariantMap colors() const;

    static Q_INVOKABLE QColor transparent(const QColor& color, qreal alpha);
    Q_INVOKABLE QColor darker(const QColor& color, int offset) const;
    Q_INVOKABLE QColor lighter(const QColor& color, int offset) const;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
