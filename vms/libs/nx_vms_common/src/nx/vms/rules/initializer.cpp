// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <common/common_module.h>

#include <nx/vms/rules/action_fields/substitution.h>
#include <nx/vms/rules/event_fields/keywords.h>

namespace nx::vms::rules {

Initializer::Initializer(QnCommonModule* common):
    Plugin(common->vmsRulesEngine()),
    m_common(common)
{
}

Initializer::~Initializer()
{
}

void Initializer::registerFields() const
{
    registerEventField<Keywords>();
    registerActionField<Substitution>();
    //m_engine->registerEventField("nx.stringWithKeywords", [](){ return new Keywords(); });
    //m_engine->registerActionField("nx.substitution", [](){ return new Substitution(); });
}

} // namespace nx::vms::rules
