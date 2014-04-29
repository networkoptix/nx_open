#ifndef QN_PALETTE_DATA_H
#define QN_PALETTE_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QExplicitlySharedDataPointer>
#include <QtGui/QPalette>

#include <utils/common/model_functions_fwd.h>

class QnPaletteDataPrivate;

/**
 * A counterpart to <tt>QPalette</tt> that is self-contained, does not depend
 * in any way on global <tt>QApplication</tt> palette, does not implement
 * color inheritance and thus can be safely used for (de)serialization.
 */
class QnPaletteData {
public:
    QnPaletteData();
    QnPaletteData(const QPalette &palette);
    QnPaletteData(const QnPaletteData &other);
    ~QnPaletteData();

    QnPaletteData &operator=(const QnPaletteData &other);

    void swap(QnPaletteData &other) { qSwap(d, other.d); }

    void applyTo(QPalette *palette) const;

    const QColor &color(QPalette::ColorGroup group, QPalette::ColorRole role) const;
    void setColor(QPalette::ColorGroup group, QPalette::ColorRole role, const QColor &color);

    QN_FUSION_DECLARE_FUNCTIONS(QnPaletteData, (json), friend)

private:
    QExplicitlySharedDataPointer<QnPaletteDataPrivate> d;
};

Q_DECLARE_METATYPE(QnPaletteData)
Q_DECLARE_SHARED(QnPaletteData)

#endif // QN_PALETTE_DATA_H
