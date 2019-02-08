#pragma once

#include <memory>

#include "abstract_engine.h"

namespace nx::vms::server::interactive_settings {

class QmlEngine: public AbstractEngine
{
public:
    QmlEngine();
    virtual ~QmlEngine() override;

    virtual Error loadModelFromData(const QByteArray& data) override;
    virtual Error loadModelFromFile(const QString& fileName) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::server::interactive_settings
