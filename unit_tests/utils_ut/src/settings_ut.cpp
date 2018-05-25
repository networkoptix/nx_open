#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/utils/settings.h>


const QString kSettingsFilename = "test.conf";

TEST(Settings, getSimpleOption)
{
    struct Settings: nx::utils::Settings
    {
        Option<QString> option1{this, "option1", "qwerty", "Option description"};
        Option<int> option2{this, "option2", 7, "Option description"};
    };

    Settings settings;
    QFile file (kSettingsFilename);
    file.remove();
    QSettings qSettings(kSettingsFilename, QSettings::IniFormat);
    qDebug() << qSettings.allKeys();
    ASSERT_TRUE(settings.load(qSettings));
    ASSERT_EQ(settings.option1(), QString("qwerty"));
    ASSERT_EQ(settings.option2(), 7);
    ASSERT_FALSE(settings.option1.present());
    ASSERT_FALSE(settings.option1.removed());
    ASSERT_FALSE(settings.option2.present());
    ASSERT_FALSE(settings.option2.removed());
}

TEST(Settings, loadSave)
{
    struct Settings: nx::utils::Settings
    {
        Option<QString> option1{this, "option1", "qwerty", "Option description"};
        Option<int> option2{this, "option2", 7, "Option description"};
    };

    Settings settings;
    QFile file (kSettingsFilename);
    file.remove();
    QSettings qSettings(kSettingsFilename, QSettings::IniFormat);
    qSettings.setValue("option1", "loaded value");
    ASSERT_TRUE(settings.load(qSettings));
    ASSERT_FALSE(settings.option2.removed());
    ASSERT_TRUE(settings.option1.present());
    ASSERT_EQ(settings.option1(), QString("loaded value"));
    ASSERT_FALSE(settings.option2.present());
    settings.option1.set("qqrq");
    ASSERT_EQ(settings.option1(), QString("qqrq"));
    ASSERT_TRUE(settings.save(qSettings));
    ASSERT_TRUE(qSettings.contains("option1"));
    ASSERT_EQ(qSettings.value("option1"), QString("qqrq"));
    ASSERT_FALSE(qSettings.contains("option2"));
    settings.option1.remove();
    ASSERT_TRUE(settings.option1.removed());
    ASSERT_TRUE(settings.save(qSettings));
    ASSERT_FALSE(qSettings.contains("option1"));
    ASSERT_FALSE(qSettings.contains("option2"));
}

TEST(Settings, getWithLambda)
{
    struct Settings: nx::utils::Settings
    {
        Option<QString> option1{this, "option1", "", "Option description",
            [this](const QString& value)
            {
                if (!value.isEmpty())
                    return value;

                return option3();
            }
        };
        Option<int> option2{this, "option2", 7, "Option description",
            [this](const int& value)
            {
                if (!option2.present())
                    return 8;

                return value;
            }
        };
        Option<QString> option3{this, "option3", "qwerty", "Option description"};
    };

    QFile file (kSettingsFilename);
    file.remove();
    QSettings qSettings(kSettingsFilename, QSettings::IniFormat);
    qSettings.setValue("option2", 10);
    Settings settings;
    ASSERT_TRUE(settings.load(qSettings));
    ASSERT_EQ(settings.option1(), QString("qwerty"));
    ASSERT_EQ(settings.option2(), 10);
}
