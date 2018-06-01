#pragma once

#include "abstract_service_settings.h"
#include "settings.h"

namespace nx {
namespace utils {

class NX_UTILS_API BasicServiceSettings:
    public AbstractServiceSettings
{
public:
    BasicServiceSettings(
        const QString& organizationName,
        const QString& applicationName,
        const QString& moduleName);

    virtual void load(int argc, const char **argv) override;
    virtual bool isShowHelpRequested() const override;
    virtual void printCmdLineArgsHelp() override;

protected:
    virtual void loadSettings() = 0;

    const QnSettings& settings() const;

private:
    QnSettings m_settings;
    bool m_showHelp;
};

} // namespace utils
} // namespace nx
