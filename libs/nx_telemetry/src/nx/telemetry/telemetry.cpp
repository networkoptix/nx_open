// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "telemetry.h"

#include <memory>

#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/resource_detector.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>

#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>

#include "settings.h"

namespace nx::telemetry {

void init(const InitAttributes& attributes, const Settings& settings)
{
    using namespace opentelemetry;
    using nostd::shared_ptr;

    if (settings.endpoint.empty())
        return;

    if (!dynamic_cast<trace::NoopTracerProvider*>(trace::Provider::GetTracerProvider().get())) {
        NX_WARNING(NX_SCOPE_TAG,
            "Skipping telemetry initialization for %1. It has already been initialized.",
            attributes.serviceName);
        return;
    }

    NX_INFO(NX_SCOPE_TAG, "Enabling telemetry reporting for %1. Settings: %2.",
        attributes.serviceName, nx::reflect::json::serialize(settings));

    exporter::otlp::OtlpGrpcExporterOptions otlpOptions;
    otlpOptions.endpoint = settings.endpoint;

    auto exporter = std::make_unique<exporter::otlp::OtlpGrpcExporter>(otlpOptions);
    auto processor = std::make_unique<sdk::trace::BatchSpanProcessor>(
        std::move(exporter),
        sdk::trace::BatchSpanProcessorOptions{
            .max_queue_size = settings.maxQueueSize,
            .schedule_delay_millis = settings.scheduleDelay,
            .max_export_batch_size = settings.maxExportBatchSize,
        }
    );

    auto resource = sdk::resource::OTELResourceDetector().Detect();
    resource = resource.Merge(
        sdk::resource::Resource::Create({
            {"service.name", attributes.serviceName},
            {"service.version", attributes.serviceVersion},
            {"deployment.environment", settings.environment},
        }));

    auto provider = shared_ptr<trace::TracerProvider>(
        new sdk::trace::TracerProvider(std::move(processor), resource));

    trace::Provider::SetTracerProvider(provider);

    context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
        shared_ptr(new trace::propagation::HttpTraceContext()));
}

} // namespace nx::telemetry
