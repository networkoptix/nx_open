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
    std::shared_ptr<QSettings> qSettings(new QSettings(kSettingsFilename, QSettings::IniFormat));
    settings.attach(qSettings);
    ASSERT_EQ(settings.option1(), QString("qwerty"));
    ASSERT_EQ(settings.option2(), 7);
    ASSERT_FALSE(settings.option1.present());
    ASSERT_FALSE(settings.option2.present());
}

TEST(Settings, loadSave)
{
    struct Settings: nx::utils::Settings
    {
        Option<QString> option1{this, "option1", "qwerty", "Option description"};
        Option<int> option2{this, "option2", 7, "Option description"};
        Option<std::chrono::milliseconds> option3{this, "option3",
            std::chrono::milliseconds(7), "Option description"};
    };

    Settings settings;
    QFile file (kSettingsFilename);
    file.remove();
    std::shared_ptr<QSettings> qSettings(new QSettings(kSettingsFilename, QSettings::IniFormat));
    qSettings->setValue("option1", "loaded value");
    qSettings->setValue("option3", "300");
    settings.attach(qSettings);
    ASSERT_TRUE(settings.option1.present());
    ASSERT_EQ(settings.option1(), QString("loaded value"));
    ASSERT_FALSE(settings.option2.present());
    ASSERT_TRUE(settings.option3.present());
    ASSERT_EQ(settings.option3().count(), 300);
    settings.option1.set("qqrq");
    ASSERT_EQ(settings.option1(), QString("qqrq"));
    ASSERT_TRUE(qSettings->contains("option1"));
    ASSERT_EQ(qSettings->value("option1"), QString("qqrq"));
    ASSERT_FALSE(qSettings->contains("option2"));
    settings.option1.remove();
    ASSERT_FALSE(qSettings->contains("option1"));
    ASSERT_FALSE(qSettings->contains("option2"));
    file.remove();
}

TEST(Settings, stringWithCommas)
{
    struct Settings: nx::utils::Settings
    {
        Option<QString> option1{this, "option1", "qwerty", "Option description"};
    };

    QFile file (kSettingsFilename);
    QString stringWithCommas = "info,debug[nx::network],verbose[nx::utils , nx::vms::server]";
    file.remove();
    {
        Settings settings;
        std::shared_ptr<QSettings> qSettings(new QSettings(kSettingsFilename, QSettings::IniFormat));
        qSettings->setValue("option1", stringWithCommas);
        settings.attach(qSettings);
        ASSERT_EQ(settings.option1(), stringWithCommas);
        qSettings->sync();
    }

    {
        std::shared_ptr<QSettings> qSettings(new QSettings(kSettingsFilename, QSettings::IniFormat));
        Settings settings;
        settings.attach(qSettings);
        ASSERT_EQ(settings.option1(), stringWithCommas);
    }

    {
        QFile file(kSettingsFilename);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write("option1 = info,debug[nx::network],verbose[nx::utils , nx::vms::server]");
        file.close();
        std::shared_ptr<QSettings> qSettings(new QSettings(kSettingsFilename, QSettings::IniFormat));
        Settings settings;
        settings.attach(qSettings);
        ASSERT_EQ(settings.option1().remove(' '), stringWithCommas.remove(' '));
    }
    file.remove();
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

    QFile file(kSettingsFilename);
    file.remove();
    std::shared_ptr<QSettings> qSettings(new QSettings(kSettingsFilename, QSettings::IniFormat));
    qSettings->setValue("option2", 10);
    Settings settings;
    settings.attach(qSettings);
    ASSERT_EQ(settings.option1(), QString("qwerty"));
    ASSERT_EQ(settings.option2(), 10);
    file.remove();
}
