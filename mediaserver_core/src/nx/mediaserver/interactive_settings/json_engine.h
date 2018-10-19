#pragma once

#include <memory>

#include "abstract_engine.h"

namespace nx::mediaserver::interactive_settings {

class JsonEngine: public AbstractEngine
{
    using base_type = AbstractEngine;

public:
    JsonEngine(QObject* parent = nullptr);
    virtual ~JsonEngine() override;

    using AbstractEngine::load;
    void load(const QJsonObject& json);
    virtual void load(const QByteArray& data) override;
};

} // namespace nx::mediaserver::interactive_settings
