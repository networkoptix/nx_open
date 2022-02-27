// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/plugin.h>

class QnCommonModule;

namespace nx::vms::rules {

class Initializer: public Plugin
{

public:
    Initializer(QnCommonModule* common);
    ~Initializer();

    void registerFields() const override;

private:
    QnCommonModule* m_common;
};

} // namespace nx::vms::rules