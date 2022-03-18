// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gmock/gmock.h>

#include <nx/vms/rules/engine.h>

namespace nx::vms::rules::test {

class MockEngineEvents: public QObject
{
    Q_OBJECT

public:
    MOCK_METHOD(void, onRuleAddedOrUpdated, (QnUuid ruleId, bool added));
    MOCK_METHOD(void, onRuleRemoved, (QnUuid ruleId));
    MOCK_METHOD(void, onRulesReset, ());

    MockEngineEvents(Engine* engine):
        m_engine(engine)
    {
        connect(m_engine, &Engine::ruleAddedOrUpdated, this, &MockEngineEvents::onRuleAddedOrUpdated);
        connect(m_engine, &Engine::ruleRemoved, this, &MockEngineEvents::onRuleRemoved);
        connect(m_engine, &Engine::rulesReset, this, &MockEngineEvents::onRulesReset);
    }

private:
    Engine* m_engine;
};

} // namespace nx::vms::rules::test
