// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>

#include "color_substitutions.h"

namespace nx::vms::client::desktop {

/**
 * QPalette analogue.
 * Read basic and skin colors and provide access to them.
 */
class NX_VMS_CLIENT_DESKTOP_API ColorTheme: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantMap colors READ colors CONSTANT)

public:
    explicit ColorTheme(QObject* parent = nullptr);
    explicit ColorTheme(
        const QString& mainColorsFile,
        const QString& skinColorsFile,
        QObject* parent = nullptr);
    virtual ~ColorTheme() override;

    static ColorTheme* instance();

    QVariantMap colors() const;

    /**
     * @return Pairs of basic and current skin color values.
     */
    ColorSubstitutions getColorSubstitutions() const;

    QColor color(const QString& name) const;
    QColor color(const QString& name, std::uint8_t alpha) const;

    QList<QColor> colors(const QString& name) const;

    static Q_INVOKABLE QColor transparent(const QColor& color, qreal alpha);
    Q_INVOKABLE QColor darker(const QColor& color, int offset) const;
    Q_INVOKABLE QColor lighter(const QColor& color, int offset) const;

    Q_INVOKABLE static bool isDark(const QColor& color);
    Q_INVOKABLE static bool isLight(const QColor& color);

    static void registerQmlType();

private:
    struct Private;
    QScopedPointer<Private> const d;
};

inline ColorTheme* colorTheme() { return ColorTheme::instance(); }

} // namespace nx::vms::client::desktop
