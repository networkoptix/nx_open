// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QDirIterator>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/utils/external_resources.h>

namespace nx::vms::client::desktop {
namespace test {

class ColorizedIconsTest: public testing::Test
{
public:
    ColorizedIconsTest()
    {
        // TODO: Perhaps we should just register all resources once in the main().
        nx::vms::utils::registerExternalResource("client_external.dat");
        nx::vms::utils::registerExternalResource("client_core_external.dat");

        // Initialize ColorTheme.
        m_colorTheme = std::make_unique<core::ColorTheme>();

        // Initialize Skin instance.
        static QString prefix = ":/skin/";
        m_skin = std::make_unique<core::Skin>(QStringList{prefix});

        // Collect all resources located in :/skin/ folder.
        QDirIterator it(prefix, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            QString path = it.next();

            if (path.endsWith(".png"))
                m_skinPngFiles << path.mid(prefix.length());

            if (path.endsWith(".svg"))
                m_skinSvgFiles << path.mid(prefix.length());
        }
    }

    virtual ~ColorizedIconsTest()
    {
        nx::vms::utils::unregisterExternalResource("client_core_external.dat");
        nx::vms::utils::unregisterExternalResource("client_external.dat");
    }

public:
    QSet<QString> pngPaths() const { return m_skinPngFiles; }

    QSet<QString> svgPaths() const { return m_skinSvgFiles; }

private:
    std::unique_ptr<core::ColorTheme> m_colorTheme;
    std::unique_ptr<core::Skin> m_skin;
    QSet<QString> m_skinPngFiles;
    QSet<QString> m_skinSvgFiles;
};

TEST_F(ColorizedIconsTest, integrityCheck)
{
    auto storage = nx::vms::client::core::ColorizedIconDeclaration::storage();

    ASSERT_FALSE(storage->isEmpty());

    for (auto it: *storage)
    {
        ASSERT_TRUE(svgPaths().contains(it->iconPath()));
        ASSERT_FALSE(qnSkin->icon(*it).isNull());
    }
}

} // namespace test
} // namespace nx::vms::client::desktop
