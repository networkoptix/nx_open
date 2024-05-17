// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/i18n/translation_manager.h>

namespace nx::i18n::test {

namespace {

class TranslationManagerOpen: public TranslationManager
{
    using base_type = TranslationManager;
public:
    static void setCurrentThreadLocale(QString locale)
    {
        base_type::setCurrentThreadLocale(locale);
    }

    static void eraseCurrentThreadLocale()
    {
        base_type::eraseCurrentThreadLocale();
    }
};

static const QString kSampleLocale = "en_US";

void testLocaleInThread(QString locale)
{
    EXPECT_EQ(TranslationManager::getCurrentThreadLocale(), QString());

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TranslationManagerOpen::setCurrentThreadLocale(locale);
    EXPECT_EQ(TranslationManager::getCurrentThreadLocale(), locale);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TranslationManagerOpen::eraseCurrentThreadLocale();
    EXPECT_EQ(TranslationManager::getCurrentThreadLocale(), QString());
}

} // namespace

TEST(TranslationManager, testLocaleWithSingleThread)
{
    testLocaleInThread(kSampleLocale);
}

TEST(TranslationManager, testLocaleWithMultipleThreads)
{
    static const int kThreadCount = 100;
    std::array<std::thread, kThreadCount> threads;

    for (int i = 0; i < kThreadCount; ++i)
    {
        const QString locale = kSampleLocale + QString::number(i);
        threads[i] = std::thread(testLocaleInThread, locale);
    }
    for (auto& thread: threads)
        thread.join();
}

} // namespace nx::i18n::test
