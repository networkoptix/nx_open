#pragma once

#include <atomic>

#include <nx/utils/argument_parser.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

class NX_UTILS_API TestOptions
{
public:
    static void setTimeoutMultiplier(size_t value);
    static size_t timeoutMultiplier();

    static void disableTimeAsserts(bool areDisabled = true);
    static bool areTimeAssertsDisabled();

    static void setTemporaryDirectoryPath(const QString& path);
    static QString temporaryDirectoryPath(bool canCreate = false);

    static void setLoadMode(const QString& mode);

    template<typename Count>
    static Count applyLoadMode(Count count);

    static void applyArguments(const ArgumentParser& args);

private:
    static std::atomic<size_t> s_timeoutMultiplier;
    static std::atomic<bool> s_disableTimeAsserts;
    static std::atomic<size_t> s_loadMode;

    struct TemporaryDirectory
    {
        TemporaryDirectory();
        ~TemporaryDirectory();

        QString path(bool canCreate = false) const;
        void setPath(const QString& path);

    private:
        mutable QnMutex m_mutex;
        QString m_path;
    };
    static TemporaryDirectory s_temporaryDirectory;
};

template<typename Count>
Count TestOptions::applyLoadMode(Count count)
{
    count *= (Count) s_loadMode;
    return (count < 1) ? 1 : count;
}

} // namespace utils
} // namespace nx
