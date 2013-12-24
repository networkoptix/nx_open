#ifndef QN_PALETTE_DATA_H
#define QN_PALETTE_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QExplicitlySharedDataPointer>
#include <QtGui/QPalette>

#include <utils/common/json_fwd.h>

class QnPaletteDataPrivate;

/**
 * A counterpart to <tt>QPalette</tt> that is self-contained, does not depend
 * in any way on global <tt>QApplication</tt> palette, does not implement
 * color inheritance and thus can be safely used for (de)serialization.
 */
class QnPaletteData {
public:
    QnPaletteData(const QPalette &palette);
    QnPaletteData();
    ~QnPaletteData();

    void apply(QPalette *palette);

    const QColor &color(QPalette::ColorGroup group, QPalette::ColorRole role) const;
    void setColor(QPalette::ColorGroup group, QPalette::ColorRole role, const QColor &color);

    QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnPaletteData, friend)

private:
    QExplicitlySharedDataPointer<QnPaletteDataPrivate> d;
};
Q_DECLARE_METATYPE(QnPaletteData)

#endif // QN_PALETTE_DATA_H
