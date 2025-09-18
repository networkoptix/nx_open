// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <nx/reflect/enum_instrument.h>

namespace nx::telemetry {

struct SpanData;

class NX_TELEMETRY_API Span
{
public:
    class NX_TELEMETRY_API Scope
    {
    public:
        Scope();
        Scope(Scope&& other);
        ~Scope();

        void exit();

        Scope& operator=(Scope&& other);

    private:
        struct Data;
        std::unique_ptr<Data> m_data;

    private:
        Scope(std::unique_ptr<Data> data);

        friend class Span;
    };

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Kind, internal, server, client, producer, consumer)
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status, empty, ok, error)

    Span();
    Span(std::string_view name, Kind kind = Kind::internal);
    Span(std::string_view name, const Span& parent, Kind kind = Kind::internal);
    virtual ~Span();

    bool isValid() const;
    std::string traceId() const;

    void updateName(std::string_view newName);

    void setAttribute(std::string_view name, std::string_view value);
    void setAttribute(std::string_view name, int value);

    void setStatus(Status status, std::string_view description = {});

    void end();

    Scope activate() const;

    std::string toDebugString() const;

    static Span activeSpan();

protected:
    Span(std::shared_ptr<SpanData> data);

protected:
    std::shared_ptr<SpanData> m_data;
};

} // namespace nx::telemetry
