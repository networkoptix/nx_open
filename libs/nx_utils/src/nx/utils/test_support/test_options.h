// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    static void setLoadFactor(double value);

    /**
     * @return Multiplier to be applied by load tests to adjust the load.
     */
    static double loadFactor();

    static void disableTimeAsserts(bool areDisabled = true);
    static bool areTimeAssertsDisabled();

    static bool keepTemporaryDirectory();
    static void setKeepTemporaryDirectory(bool value);
    static void setTemporaryDirectoryPath(const QString& path);
    static QString temporaryDirectoryPath(bool canCreate = false);

    static void setModuleName(const QString& value);

    /**
     * @return Random unique string, unless set by TestOptions::setModuleName.
     */
    static QString moduleName();

    static void setLoadMode(const QString& mode);

    template<typename Count>
    static Count applyLoadMode(Count count);

    static void applyArguments(const ArgumentParser& args);

private:
    struct TemporaryDirectory
    {
        TemporaryDirectory();
        ~TemporaryDirectory();

        QString path(bool canCreate = false) const;
        void setPath(const QString& path);

    private:
        mutable nx::Mutex m_mutex;
        QString m_path;
    };

    static TemporaryDirectory& tmpDirInstance();

private:
    static std::atomic<size_t> s_timeoutMultiplier;
    static std::atomic<double> s_loadFactor;
    static std::atomic<bool> s_disableTimeAsserts;
    static std::atomic<bool> s_keepTemporaryDirectory;
    static std::atomic<size_t> s_loadMode;

    static QString s_moduleName;
};

template<typename Count>
Count TestOptions::applyLoadMode(Count count)
{
    count *= (Count) s_loadMode;
    return (count < 1) ? 1 : count;
}

} // namespace utils
} // namespace nx
