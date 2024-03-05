// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_service_settings.h"
#include "deprecated_settings.h"

namespace nx::utils {

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
    virtual QString dataDir() const override;
    virtual log::Settings logging() const override;

protected:
    virtual void loadSettings() = 0;

    const QnSettings& settings() const;

private:
    QnSettings m_settings;
    bool m_showHelp = false;
    log::Settings m_logging;
    QString m_moduleName;
};

NX_REFLECTION_INSTRUMENT_GSN(BasicServiceSettings,
    ((&BasicServiceSettings::dataDir, nx::reflect::none, "dataDir")))

} // namespace nx::utils
