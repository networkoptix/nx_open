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

    void load(const QJsonObject& json);
    virtual void load(const QByteArray& data) override;
    virtual void load(const QUrl& url) override;
};

} // namespace nx::mediaserver::interactive_settings
