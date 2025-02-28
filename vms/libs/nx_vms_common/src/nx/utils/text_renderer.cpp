// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_renderer.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtGui/QGuiApplication>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <nx/build_info.h>
#include <nx/utils/log/log.h>

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace nx::utils {

class FreetypeTextRenderer: public AbstractTextRenderer
{
public:
    FreetypeTextRenderer()
    {
        m_initialized = FT_Init_FreeType(&m_ft) == 0;
        if (!m_initialized)
            NX_WARNING(NX_SCOPE_TAG, "Could not initialize FreeType library.");
    }

    virtual ~FreetypeTextRenderer() override
    {
        if (m_initialized)
            FT_Done_FreeType(m_ft);
    }

    virtual void drawText(QImage& image, const QRect& rect, const QString& text) override
    {
        FT_Face face;
        if (FT_New_Face(m_ft, fontPath().toUtf8().data(), 0, &face))
        {
            NX_WARNING(NX_SCOPE_TAG, "Could not load font face.");
            return;
        }
        FT_Set_Pixel_Sizes(face, 0, fontPixelSize);

        unsigned char* rgbaBuffer = image.bits();
        int penX = rect.left() * 64;
        int penY = rect.top() * 64;

        for (const auto& ch: text)
        {
            const FT_ULong code = (FT_ULong) ch.unicode();
            // Load glyph into the face's glyph slot and render it to a bitmap
            if (FT_Load_Char(face, code, FT_LOAD_RENDER))
            {
                NX_WARNING(NX_SCOPE_TAG, "Failed to load Glyph for character: %1", ch);
                continue;
            }

            const int maxWidth = rect.right() - penX / 64;
            const int maxHeight = rect.bottom() - penY / 64;

            FT_GlyphSlot glyph = face->glyph;
            blendGlyphToBuffer(
                rgbaBuffer,
                image.width(), image.height(),
                &glyph->bitmap,
                penX / 64, penY / 64,
                maxWidth, maxHeight,
                color.red(), color.green(), color.blue());
            penX += glyph->advance.x; //< Fixed point format, low 6 bits are subpixels.
            penY += glyph->advance.y;
        }
    }

    virtual QSize textSize(const QString& text) override
    {
        FT_Face face;
        if (FT_New_Face(m_ft, fontPath().toUtf8().data(), 0, &face))
        {
            NX_WARNING(NX_SCOPE_TAG, "Could not load font face.");
            return QSize();
        }
        FT_Set_Pixel_Sizes(face, 0, fontPixelSize);

        int width = 0;
        int height = 0;

        for (const auto& ch: text)
        {
            const FT_ULong code = (FT_ULong) ch.unicode();
            if (FT_Load_Char(face, code, FT_LOAD_RENDER))
            {
                NX_WARNING(NX_SCOPE_TAG, "Failed to load Glyph for character: %1", ch);
                continue;
            }

            FT_Glyph glyph;
            if (FT_Get_Glyph(face->glyph, &glyph) == 0)
            {
                FT_BBox bbox;
                FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_TRUNCATE, &bbox);

                width += face->glyph->advance.x;
                height += face->glyph->advance.y;
                width = std::max(width, int(bbox.xMax - bbox.xMin) * 64);
                height = std::max(height, int(bbox.yMax - bbox.yMin) * 64);

                FT_Done_Glyph(glyph);
            }
        }

        return QSize(width / 64, height / 64);
    }

private:
    QString fontPath() const
    {
        const QDir appDir(QCoreApplication::applicationDirPath());

        const QString fontDir = nx::build_info::isMacOsX()
            ? appDir.absoluteFilePath("../Resources/fonts")
            : appDir.absoluteFilePath("fonts");

        return nx::format("%1/%2.ttf", fontDir, fontFamily);
    }

    void blendGlyphToBuffer(
        unsigned char* buffer,
        int bufferWidth, int bufferHeight,
        FT_Bitmap* bitmap,
        int xOffset, int yOffset,
        int maxX, int maxY,
        unsigned char r, unsigned char g, unsigned char b)
    {
        // Loop over each pixel of the glyph bitmap.
        for (int row = 0; row < std::min((int) bitmap->rows, maxY); ++row)
        {
            for (int col = 0; col < std::min((int) bitmap->width, maxX); ++col)
            {
                const int bufferX = xOffset + col;
                const int bufferY = yOffset + row;
                if (bufferX < 0 || bufferX >= bufferWidth || bufferY < 0 || bufferY >= bufferHeight)
                    continue;

                // Get the alpha value from the glyph's bitmap (0-255).
                const uint8_t glyphAlpha = bitmap->buffer[row * bitmap->pitch + col];
                if (glyphAlpha == 0)
                    continue;

                // Calculate buffer index (RGBA: 4 bytes per pixel).
                const int bufferIndex = (bufferY * bufferWidth + bufferX) * 4;

                // Blend the glyph pixel with the background using simple alpha blending.
                // Here, the background is assumed to be black and fully transparent.
                const float alpha = glyphAlpha / 255.0f;
                buffer[bufferIndex + 0] = uint8_t(r * alpha);
                buffer[bufferIndex + 1] = uint8_t(g * alpha);
                buffer[bufferIndex + 2] = uint8_t(b * alpha);
                // Set alpha channel to glyph's alpha.
                buffer[bufferIndex + 3] = glyphAlpha;
            }
        }
    }

private:
    FT_Library m_ft;
    bool m_initialized = false;
};

} // namespace nx::utils

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

namespace nx::utils {

class QtTextRenderer: public AbstractTextRenderer
{
public:
    virtual void drawText(QImage& image, const QRect& rect, const QString& text) override
    {
        QFont font(fontFamily);
        font.setPixelSize(fontPixelSize);

        QPainter painter(&image);
        painter.setFont(font);
        painter.setPen(color);

        painter.drawText(rect, /*flags*/ 0, text);
    }

    virtual QSize textSize(const QString& text) override
    {
        QFont font(fontFamily);
        font.setPixelSize(fontPixelSize);
        return QFontMetrics(font).boundingRect(text).size();
    }
};

std::unique_ptr<AbstractTextRenderer> TextRendererFactory::create()
{
    std::unique_ptr<AbstractTextRenderer> renderer;

    // Rendering via freeType could has issue with unicode characters due to
    // lack of symbols in the font. So, try Qt renderer first if accessible.
    if (qobject_cast<QGuiApplication*>(qApp))
        renderer = std::make_unique<QtTextRenderer>();

    #if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
        if (!renderer)
            renderer = std::make_unique<FreetypeTextRenderer>();
    #endif

    return renderer;
}

} // nx::utils
