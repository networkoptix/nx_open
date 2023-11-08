// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtGui/QIcon>
#include <QtQml/QJSValue>

#include "color_substitutions.h"

namespace nx::vms::client::core {

/**
 * QPalette analogue.
 * Read basic and skin colors and provide access to them.
 */
class NX_VMS_CLIENT_CORE_API ColorTheme: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QJSValue colors READ jsColors CONSTANT)

public:
    explicit ColorTheme(QObject* parent = nullptr);
    explicit ColorTheme(
        const QString& mainColorsFile,
        const QString& skinColorsFile,
        QObject* parent = nullptr);
    virtual ~ColorTheme() override;

    static ColorTheme* instance();

    /**
     * @return Pairs of basic and current skin color values.
     */
    ColorSubstitutions getColorSubstitutions() const;

    QColor color(const QString& name) const;
    QColor color(const QString& name, std::uint8_t alpha) const;

    QList<QColor> colors(const QString& name) const;

    QVariantMap colors() const;

    using ImageCustomization = QMap<QString /*source class name*/, QString /*target color name*/>;
    using IconCustomization = QHash<QPair<QIcon::State, QIcon::Mode>, ImageCustomization>;

    bool hasIconCustomization(const QString& category) const;
    IconCustomization iconCustomization(const QString& category) const;

    ImageCustomization iconImageCustomization(
        const QString& category,
        const QIcon::Mode mode,
        const QIcon::State state) const;

    Q_INVOKABLE QJSValue /*ImageCustomization*/ iconImageCustomization(const QString& category,
        bool hovered, bool pressed, bool selected, bool disabled, bool checked) const;

    static Q_INVOKABLE QColor transparent(const QColor& color, qreal alpha);
    static Q_INVOKABLE QColor blend(const QColor& background, const QColor& foreground, qreal alpha);
    Q_INVOKABLE QColor darker(const QColor& color, int offset) const;
    Q_INVOKABLE QColor lighter(const QColor& color, int offset) const;

    Q_INVOKABLE static bool isDark(const QColor& color);
    Q_INVOKABLE static bool isLight(const QColor& color);

    static void registerQmlType();

private:
    QJSValue jsColors() const;

private:
    struct Private;
    QScopedPointer<Private> const d;
};

inline ColorTheme* colorTheme() { return ColorTheme::instance(); }

} // namespace nx::vms::client::core
