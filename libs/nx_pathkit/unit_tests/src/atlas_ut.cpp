// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <fstream>

#include <nx/pathkit/atlas.h>

namespace nx::pathkit {

namespace test {
namespace {

class SvgRender
{
public:
    SvgRender(const std::string& name, int w, int h)
    {
        m_output.open(name);
        m_output << "<svg width=\"" << w << "\" height=\"" << h
            << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";

        addRect(0, 0, w, h, "grey", "grey");
    }

    ~SvgRender()
    {
        m_output << "</svg>\n";
        m_output.close();
    }

    void rect(
        int x,
        int y,
        int w,
        int h,
        const std::string& fill = "blue",
        const std::string& stroke = "black")
    {
        addRect(x, y, w, h, fill, stroke);
    }

    void line(int x1, int y1, int x2, int y2, const std::string& stroke = "black")
    {
        m_output << "<line x1=\"" << x1 << "\" y1=\"" << y1
            << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\" stroke=\"" << stroke << "\" />\n";
    }


private:
    void addRect(int x, int y, int w, int h, const std::string& fill, const std::string& stroke)
    {
        m_output << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\""
            << w << "\" height=\"" << h << "\" stroke=\""
            << stroke << "\" stroke-width=\"1\" fill=\"" << fill << "\" />\n";
    }

    std::ofstream m_output;
};

void dumpAtlas(Atlas& atlas, const std::string& name)
{
    SvgRender svg(name, atlas.width(), atlas.height());

    int lastLine = -1;

    atlas.removeIf(
        [&](const Atlas::Rect& r)
        {
            if (r.y != lastLine)
            {
                svg.line(0, r.y, atlas.width(), r.y);
                lastLine = r.y;
            }
            svg.rect(r.x, r.y, r.w, r.h);
            return false;
        });
}

using AtlasData = std::vector<Atlas::Rect>;

AtlasData getData(Atlas& atlas)
{
    AtlasData result;

    atlas.removeIf(
        [&](const Atlas::Rect& r)
        {
            result.push_back(r);
            return false;
        });

    return result;
}

} // namespace

TEST(AtlasTest, avoidWasteIf2xSmaller)
{
    Atlas atlas(64, 64);

    atlas.add(32, 32);
    atlas.add(16, 16); //< New shelf.

    const AtlasData data{
        {0, 0, 32, 32},
        {0, 32, 16, 16}};

    ASSERT_EQ(data, getData(atlas));
}

TEST(AtlasTest, sameShelfIfNoSpace)
{
    Atlas atlas(100, 40);

    atlas.add(32, 32);
    atlas.add(16, 16); //< Same shelf because of no space.

    const AtlasData data{
        {0, 0, 32, 32},
        {32, 0, 16, 16}};

    ASSERT_EQ(data, getData(atlas));
}

TEST(AtlasTest, newShelfIfNoSpace)
{
    Atlas atlas(20, 40);

    atlas.add(10, 10);
    atlas.add(10, 10);
    atlas.add(10, 10); //< New shelf because of no space in the first one.

    const AtlasData data{
        {0, 0, 10, 10},
        {10, 0, 10, 10},
        {0, 10, 10, 10}};

    ASSERT_EQ(data, getData(atlas));
}

TEST(AtlasTest, fullAfter4Rect)
{
    Atlas atlas(20, 20);

    ASSERT_FALSE(atlas.add(10, 10).isNull());
    ASSERT_FALSE(atlas.add(10, 10).isNull());
    ASSERT_FALSE(atlas.add(10, 10).isNull());
    ASSERT_FALSE(atlas.add(10, 10).isNull());

    ASSERT_TRUE(atlas.add(10, 10).isNull());
}

TEST(AtlasTest, checkSvg)
{
    Atlas atlas(512, 512);

    auto r0 = atlas.add(32, 32);
    auto r1 = atlas.add(32, 32); //< Same shelf as r0.
    /*auto r2 = */ atlas.add(16, 16); //< New shelf.
    /*auto r3 = */ atlas.add(20, 20); //< Same shelf as r0, r1.
    auto r4 = atlas.add(24, 36); //< New shelf.
    auto r5 = atlas.add(24, 40); //< New shelf.
    /*auto r6 =*/ atlas.add(24, 50); //< New shelf.

    dumpAtlas(atlas, "atlas1.svg");
    atlas.remove(r4);
    dumpAtlas(atlas, "atlas2.svg");
    atlas.remove(r5);
    dumpAtlas(atlas, "atlas3.svg");
    atlas.add(24, 55);
    dumpAtlas(atlas, "atlas4.svg");
    atlas.remove(r0);
    dumpAtlas(atlas, "atlas5.svg");
    atlas.add(16, 16);
    dumpAtlas(atlas, "atlas6.svg");
    atlas.remove(r1);
    dumpAtlas(atlas, "atlas7.svg");
    atlas.add(40, 16);
    dumpAtlas(atlas, "atlas8.svg");
}

} // namespace test

} // namespace nx::pathkit
