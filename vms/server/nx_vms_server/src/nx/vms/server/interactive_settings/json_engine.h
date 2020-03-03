#pragma once

#include "abstract_engine.h"

namespace nx::vms::server::interactive_settings {

class JsonEngine: public AbstractEngine
{
public:
    JsonEngine();

    bool loadModelFromJsonObject(const QJsonObject& json);
    virtual bool loadModelFromData(const QByteArray& data) override;
};

} // namespace nx::vms::server::interactive_settings
