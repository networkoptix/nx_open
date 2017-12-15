#pragma once

#include <QtCore/QString>

#include "log/log_settings.h"

namespace nx {
namespace utils {

class NX_UTILS_API AbstractServiceSettings
{
public:
    virtual ~AbstractServiceSettings() = default;

    virtual void load(int argc, const char **argv) = 0;
    virtual bool isShowHelpRequested() const = 0;
    virtual void printCmdLineArgsHelp() = 0;

    /**
     * Writable directory that service uses as a persistent storage.
     */
    virtual QString dataDir() const = 0;
    virtual utils::log::Settings logging() const = 0;
};

} // namespace utils
} // namespace nx
