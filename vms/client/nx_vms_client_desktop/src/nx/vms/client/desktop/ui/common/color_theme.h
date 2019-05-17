#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>

#include <nx/utils/singleton.h>

namespace nx::vms::client::desktop {

class ColorTheme: public QObject, public Singleton<ColorTheme>
{
    Q_OBJECT

    Q_PROPERTY(QVariantMap colors READ colors CONSTANT)

public:
    explicit ColorTheme(QObject* parent = nullptr);
    virtual ~ColorTheme() override;

    QVariantMap colors() const;

    QColor color(const char* name) const;
    QColor color(const QString& name) const;

    QColor color(const char* name, qreal alpha) const;
    QColor color(const QString& name, qreal alpha) const;

    QList<QColor> groupColors(const char* groupName) const;
    QList<QColor> groupColors(const QString& groupName) const;

    static Q_INVOKABLE QColor transparent(const QColor& color, qreal alpha);
    Q_INVOKABLE QColor darker(const QColor& color, int offset) const;
    Q_INVOKABLE QColor lighter(const QColor& color, int offset) const;

    Q_INVOKABLE static bool isDark(const QColor& color);
    Q_INVOKABLE static bool isLight(const QColor& color);

private:
    struct Private;
    QScopedPointer<Private> const d;
};

inline ColorTheme* colorTheme() { return ColorTheme::instance(); }

} // namespace nx::vms::client::desktop
