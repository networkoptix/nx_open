#pragma once

#include "abstract_engine.h"

namespace nx::mediaserver::interactive_settings {

class JsonEngine: public AbstractEngine
{
public:
    JsonEngine();

    Error loadModelFromJsonObject(const QJsonObject& json);
    virtual Error loadModelFromData(const QByteArray& data) override;
};

} // namespace nx::mediaserver::interactive_settings
