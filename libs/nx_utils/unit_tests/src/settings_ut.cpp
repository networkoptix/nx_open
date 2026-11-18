// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/utils/settings.h>
#include <nx/utils/test_support/test_options.h>

using namespace nx::utils;

class TestQSettings : public nx::utils::AbstractQSettings
{
public:
    QMap<QString, QVariant> impl;

    virtual void setValue(const QString& name, const QVariant& value) override
    {
        impl.insert(name, value);
    }

    virtual void remove(const QString& name) override
    {
        impl.remove(name);
    }

    virtual QStringList allKeys() const override
    {
        return impl.keys();
    }

    virtual QVariant value(const QString& name) const override
    {
        return impl.value(name);
    }

    virtual bool contains(const QString& name) override
    {
        return impl.contains(name);
    }

    virtual void sync() override {}
    virtual void beginGroup(const QString&) override {}
    virtual void endGroup() override {}
    virtual QSettings* qSettings() override { return nullptr; }
};

TEST(Settings, getSimpleOption)
{
    struct Settings : nx::utils::Settings
    {
        Option<QString> option1{ this, "option1", "qwerty", "Option description" };
        Option<int> option2{ this, "option2", 7, "Option description" };

        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    Settings settings;
    std::shared_ptr<nx::utils::AbstractQSettings> qSettings = std::make_shared<TestQSettings>();
    settings.attach(qSettings);
    ASSERT_EQ(settings.option1(), QString("qwerty"));
    ASSERT_EQ(settings.option2(), 7);
    ASSERT_FALSE(settings.option1.present());
    ASSERT_FALSE(settings.option2.present());
}

TEST(Settings, loadSave)
{
    struct Settings : nx::utils::Settings
    {
        Option<QString> option1{ this, "option1", "qwerty", "Option description" };
        Option<int> option2{ this, "option2", 7, "Option description" };
        Option<std::chrono::milliseconds> option3{ this, "option3",
            std::chrono::milliseconds(7), "Option description" };
        Option<std::chrono::seconds> option4{ this, "option4",
            std::chrono::seconds(142), "Option description" };
        Option<std::chrono::minutes> option5{ this, "option5",
            std::chrono::minutes(242), "Option description" };

        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    Settings settings;
    std::shared_ptr<nx::utils::AbstractQSettings> qSettings = std::make_shared<TestQSettings>();
    qSettings->setValue("option1", "loaded value");
    qSettings->setValue("option3", "300");
    qSettings->setValue("option4", "400");
    qSettings->setValue("option5", "500");
    settings.attach(qSettings);
    ASSERT_TRUE(settings.option1.present());
    ASSERT_EQ(settings.option1(), QString("loaded value"));
    ASSERT_FALSE(settings.option2.present());
    ASSERT_TRUE(settings.option3.present());
    ASSERT_EQ(settings.option3().count(), 300);
    ASSERT_TRUE(settings.option4.present());
    ASSERT_EQ(settings.option4().count(), 400);
    ASSERT_TRUE(settings.option5.present());
    ASSERT_EQ(settings.option5().count(), 500);
    settings.option1.set("qqrq");
    ASSERT_EQ(settings.option1(), QString("qqrq"));
    ASSERT_TRUE(qSettings->contains("option1"));
    ASSERT_EQ(qSettings->value("option1"), QString("qqrq"));
    ASSERT_FALSE(qSettings->contains("option2"));
    settings.option1.remove();
    ASSERT_FALSE(qSettings->contains("option1"));
    ASSERT_FALSE(qSettings->contains("option2"));
}

TEST(Settings, stringWithCommas)
{
    struct Settings : nx::utils::Settings
    {
        Option<QString> option1{ this, "option1", "qwerty", "Option description" };
        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    QString stringWithCommas = "info,debug[nx::network],verbose[nx::utils , nx::vms::server]";
    {
        Settings settings;
        std::shared_ptr<nx::utils::AbstractQSettings> qSettings = std::make_shared<TestQSettings>();
        qSettings->setValue("option1", stringWithCommas);
        settings.attach(qSettings);
        ASSERT_EQ(settings.option1(), stringWithCommas);
        qSettings->sync();
    }
}

TEST(Settings, getWithLambda)
{
    struct Settings : nx::utils::Settings
    {
        Option<QString> option1{ this, "option1", "", "Option description",
            [this](const QString& value)
            {
                if (!value.isEmpty())
                    return value;

                return option3();
            }
        };
        Option<int> option2{ this, "option2", 7, "Option description",
            [this](const int& value)
            {
                if (!option2.present())
                    return 8;

                return value;
            }
        };
        Option<QString> option3{ this, "option3", "qwerty", "Option description" };

        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    std::shared_ptr<nx::utils::AbstractQSettings> qSettings = std::make_shared<TestQSettings>();
    qSettings->setValue("option2", 10);
    Settings settings;
    settings.attach(qSettings);
    ASSERT_EQ(settings.option1(), QString("qwerty"));
    ASSERT_EQ(settings.option2(), 10);
}

TEST(Settings, resetWritesOnlyIfNeeded)
{
    struct Settings: nx::utils::Settings
    {
        Option<bool> flag{this, "flag", false, "Option description"};
        Option<QString> password{this, "password", "", "Option description"};
        Option<QString> absent{this, "absent", "", "Option description"};

        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    auto testSettings = std::make_shared<TestQSettings>();
    testSettings->setValue("flag", false);
    testSettings->setValue("password", "secret");
    std::shared_ptr<nx::utils::AbstractQSettings> qSettings = testSettings;
    Settings settings;
    settings.attach(qSettings);

    // The value is the default one already, or there is no value: the settings are not touched.
    testSettings->impl.clear();
    settings.flag.reset();
    settings.absent.reset();
    ASSERT_TRUE(testSettings->impl.isEmpty());

    settings.password.reset();
    ASSERT_EQ(settings.password(), QString());
    ASSERT_EQ(testSettings->impl.value("password"), QString());
    ASSERT_EQ(testSettings->impl.size(), 1);
}

TEST(Settings, malformedNumbersKeepDefaults)
{
    struct Settings: nx::utils::Settings
    {
        Option<int> intOption{this, "intOption", 7, "Option description"};
        Option<int> intOverflowOption{this, "intOverflowOption", 7, "Option description"};
        Option<unsigned int> unsignedOption{this, "unsignedOption", 7, "Option description"};
        Option<qint64> emptyOption{this, "emptyOption", 7, "Option description"};
        Option<double> doubleOption{this, "doubleOption", 0.5, "Option description"};
        Option<std::chrono::seconds> durationOption{
            this, "durationOption", std::chrono::seconds(7), "Option description"};

        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    std::shared_ptr<nx::utils::AbstractQSettings> qSettings = std::make_shared<TestQSettings>();
    // The value from VMS-63222: it used to be loaded as 0 instead of being rejected.
    qSettings->setValue("intOption", "999=custom");
    qSettings->setValue("intOverflowOption", "99999999999");
    qSettings->setValue("unsignedOption", "-1");
    qSettings->setValue("emptyOption", "");
    qSettings->setValue("doubleOption", "abc");
    qSettings->setValue("durationOption", "1e3");
    Settings settings;
    settings.attach(qSettings);

    ASSERT_FALSE(settings.intOption.present());
    ASSERT_EQ(settings.intOption(), 7);
    ASSERT_FALSE(settings.intOverflowOption.present());
    ASSERT_EQ(settings.intOverflowOption(), 7);
    ASSERT_FALSE(settings.unsignedOption.present());
    ASSERT_EQ(settings.unsignedOption(), 7u);
    ASSERT_FALSE(settings.emptyOption.present());
    ASSERT_EQ(settings.emptyOption(), 7);
    ASSERT_FALSE(settings.doubleOption.present());
    ASSERT_EQ(settings.doubleOption(), 0.5);
    ASSERT_FALSE(settings.durationOption.present());
    ASSERT_EQ(settings.durationOption(), std::chrono::seconds(7));

    // Reported by the Server once the logging is initialized.
    ASSERT_EQ(settings.problems().size(), 6u);
    ASSERT_TRUE(std::ranges::any_of(settings.problems(),
        [](const std::string& problem)
        {
            return problem.find("intOption") != std::string::npos
                && problem.find("'999=custom'") != std::string::npos;
        }));
}

TEST(Settings, numbersAreParsed)
{
    struct Settings: nx::utils::Settings
    {
        Option<int> intOption{this, "intOption", 7, "Option description"};
        Option<int> negativeOption{this, "negativeOption", 7, "Option description"};
        Option<qint64> bigOption{this, "bigOption", 7, "Option description"};
        Option<double> doubleOption{this, "doubleOption", 0.5, "Option description"};
        Option<bool> boolOption{this, "boolOption", false, "Option description"};
        Option<std::chrono::milliseconds> negativeDurationOption{
            this, "negativeDurationOption", std::chrono::milliseconds(7), "Option description"};

        virtual void applyMigrations(std::shared_ptr<AbstractQSettings>&) override {}
    };

    std::shared_ptr<nx::utils::AbstractQSettings> qSettings = std::make_shared<TestQSettings>();
    qSettings->setValue("intOption", " 42 ");
    qSettings->setValue("negativeDurationOption", "-1");
    qSettings->setValue("negativeOption", "-3");
    qSettings->setValue("bigOption", "99999999999");
    qSettings->setValue("doubleOption", "1.25");
    qSettings->setValue("boolOption", "true");
    Settings settings;
    settings.attach(qSettings);

    ASSERT_EQ(settings.intOption(), 42);
    ASSERT_EQ(settings.negativeOption(), -3);
    ASSERT_EQ(settings.bigOption(), 99999999999LL);
    ASSERT_EQ(settings.doubleOption(), 1.25);
    ASSERT_TRUE(settings.boolOption());
    ASSERT_EQ(settings.negativeDurationOption(), std::chrono::milliseconds(-1));
    ASSERT_TRUE(settings.problems().empty());
}
