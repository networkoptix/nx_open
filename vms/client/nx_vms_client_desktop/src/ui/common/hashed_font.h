#ifndef QN_HASHED_FONT_H
#define QN_HASHED_FONT_H

#include <QtCore/QHash>
#include <QtGui/QFont>

/**
 * A <tt>QFont</tt> wrapper class that is suitable for storing in hash maps.
 */
class QnHashedFont {
public:
    QnHashedFont():
        m_font(QFont()),
        m_hash(calculateHash(QFont()))
    {}

    QnHashedFont(const QFont &font): 
        m_font(font),
        m_hash(calculateHash(font))
    {}

    operator const QFont &() {
        return m_font;
    }

    const QFont &font() const {
        return m_font;
    }

    friend uint qHash(const QnHashedFont &font) {
        return font.m_hash;
    }

    bool operator==(const QnHashedFont &other) const {
        return m_hash == other.m_hash && m_font == other.m_font;
    }

    bool operator!=(const QnHashedFont &other) const {
        return !(*this == other);
    }

private:
    static uint calculateHash(const QFont &font) {
        /* Note that QFont::key returns a string concatenation of all
         * QFont attributes. For the sake of hash calculation, our method
         * is faster. */
        using ::qHash;

        /* |31    ...    16|15|14|13 12 11 10| 9  8| 7| 6  5  4| 3| 2| 1| 0| *
         * | StyleStrategy |Rm|Fp| StyleHint |Style|Ls|  Caps  |Kr|St|Ov|Un| */
        quint32 flags = 
            (font.underline() << 0) | 
            (font.overline() << 1) |
            (font.strikeOut() << 2) |
            (font.kerning() << 3) |
            (font.capitalization() << 4) | 
            (font.letterSpacingType() << 7) | 
            (font.style() << 8) |
            (font.styleHint() << 10) |
            (font.fixedPitch() << 14) |
            (font.rawMode() << 15) |
            (font.styleStrategy() << 16);

        /* All numbers in the combination are primes. */
        quint32 combination =
            (101081 * static_cast<int>(font.wordSpacing() * 100)) ^
            (88001  * static_cast<int>(font.letterSpacing() * 100)) ^
            (10247  * static_cast<int>(font.pointSizeF() * 100)) ^
            (3533   * font.pixelSize()) ^
            (503    * font.weight()) ^
            (197    * font.stretch());

        return 
            flags ^
            combination ^
            qHash(font.family());
    }

private:
    QFont m_font;
    uint m_hash;
};

#endif // QN_HASHED_FONT_H
