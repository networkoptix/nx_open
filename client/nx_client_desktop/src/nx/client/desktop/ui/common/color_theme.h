#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

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

    QVariantMap colors() const;

private:
    void loadColors();

private:
    QVariantMap m_colors;
};

} // namespace desktop
} // namespace client
} // namespace nx
