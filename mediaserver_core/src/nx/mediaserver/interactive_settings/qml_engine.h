#pragma once

#include <memory>

#include "abstract_engine.h"

namespace nx::mediaserver::interactive_settings {

class QmlEngine: public AbstractEngine
{
    using base_type = AbstractEngine;

public:
    QmlEngine(QObject* parent = nullptr);
    virtual ~QmlEngine() override;

    virtual void load(const QByteArray& data) override;
    virtual void load(const QUrl& url) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::mediaserver::interactive_settings
