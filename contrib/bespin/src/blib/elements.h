
class QPixmap;

namespace Bespin {
namespace Elements {

    BLIB_EXPORT void setShadowIntensity(float intensity);
    BLIB_EXPORT void setScale(float scale);
    BLIB_EXPORT QImage glow(int size, float width);
    BLIB_EXPORT QImage shadow(int size, bool opaque, bool sunken, float factor = 1.0);
    BLIB_EXPORT QImage roundMask(int size);
    BLIB_EXPORT QImage roundedMask(int size, int factor);
    BLIB_EXPORT QImage sunkenShadow(int size, bool enabled);
    BLIB_EXPORT QImage relief(int size, bool enabled);
    BLIB_EXPORT QImage groupShadow(int size);

#if 0
BLIB_EXPORT void renderButtonLight(Tile::Set &set);
BLIB_EXPORT void renderLightLine(Tile::Line &line);
#endif

}
}

#undef fillRect
