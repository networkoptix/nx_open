// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "span.h"

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>

#include <nx/utils/log/log.h>

#include "private/helpers.h"
#include "private/span_data.h"

using namespace opentelemetry;

namespace nx::telemetry {

struct Span::Scope::Data
{
    Span span;
    trace::Scope scope;
};

Span::Span()
{
}

Span::Span(const std::string_view& name, Kind kind):
    Span(name, activeSpan(), kind)
{
}

Span::Span(const std::string_view& name, const Span& parent, Kind kind):
    m_data(new SpanData())
{
    auto parentContext = trace::SpanContext::GetInvalid();
    if (const auto data = parent.m_data; data && data->span)
        parentContext = data->span->GetContext();

    m_data->span = helpers::getTracer()->StartSpan(
        nostd::string_view(name.data(), name.length()),
        trace::StartSpanOptions{
            .parent = parentContext,
            .kind = helpers::toSpanKind(kind)});
    m_data->kind = kind;
    m_data->name = name;
    m_data->parentId = helpers::spanIdString(parentContext.span_id());
}

Span::Span(std::shared_ptr<SpanData> data):
    m_data(data)
{
    NX_CRITICAL(m_data);
}

Span::~Span()
{
}

bool Span::isValid() const
{
    return m_data && m_data->span;
}

std::string Span::traceId() const
{
    if (!isValid())
        return {};

    return helpers::traceIdString(m_data->span->GetContext().trace_id());
}

void Span::updateName(const std::string_view& newName)
{
    if (!isValid())
        return;

    m_data->span->UpdateName(helpers::toStringView(newName));
}

void Span::setAttribute(const std::string_view& name, const std::string_view& value)
{
    if (!isValid())
        return;

    m_data->span->SetAttribute(helpers::toStringView(name), helpers::toStringView(value));
}

void Span::setAttribute(const std::string_view& name, int value)
{
    if (!isValid())
        return;

    m_data->span->SetAttribute(helpers::toStringView(name), value);
}

void Span::setStatus(Status status, const std::string_view& description)
{
    if (!isValid())
        return;

    m_data->span->SetStatus(helpers::toStatusCode(status), helpers::toStringView(description));
}

void Span::end()
{
    if (!isValid())
        return;

    if (m_data->reference)
        return;

    m_data->span->End();
}

Span::Scope Span::activate() const
{
    if (!isValid())
        return Scope();

    return Scope(
        std::make_unique<Scope::Data>(*this, helpers::getTracer()->WithActiveSpan(m_data->span)));

}

std::string Span::toDebugString() const
{
    if (isValid())
    {
        const trace::SpanContext ctx = m_data->span->GetContext();

        return nx::format("%1: trace:%2, kind: %3, id:%4, parent: %5",
            m_data->name,
            helpers::traceIdString(ctx.trace_id()),
            m_data->kind,
            helpers::spanIdString(ctx.span_id()),
            m_data->parentId).toStdString();
    }

    return "<invalid span>";
}

Span Span::activeSpan()
{
    auto span = opentelemetry::trace::Tracer::GetCurrentSpan();
    if (span->GetContext().IsValid())
        return Span(std::make_shared<SpanData>(span, /*reference*/ true));

    return Span();
}

Span::Scope::Scope()
{
}

Span::Scope::Scope(Scope&& other):
    m_data(std::move(other.m_data))
{
}

Span::Scope::~Scope()
{
}

void Span::Scope::exit()
{
    m_data.reset();
}

Span::Scope& Span::Scope::operator=(Scope&& other)
{
    m_data = std::move(other.m_data);
    return *this;
}

Span::Scope::Scope(std::unique_ptr<Data> data): m_data(std::move(data))
{
}

} // namespace nx::telemetry
