// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

struct QnHttpConfigureRequest
{
    QString templateString;
    QString method;
    bool isAllowedToFail;
    QString body;
};

typedef std::vector<QnHttpConfigureRequest> QnHttpConfigureRequestList;

#define QnHttpConfigureRequest_Fields (templateString)(method)(isAllowedToFail)(body)
QN_FUSION_DECLARE_FUNCTIONS(QnHttpConfigureRequest, (json)(ubjson)(xml)(csv_record))

typedef std::vector<quint64> QnBitrateList;

struct TwoWayAudioParams
{
    QString engine;
    QString codec;
    int bitrateKbps = 0;
    int channels = 1;
    int sampleRate = 0;
    QString urlPath;
    QString contentType;
    bool noAuth = false;
    bool useBasicAuth = false;
    int frameSize = 0;
};
#define TwoWayAudioParams_Fields (engine)(codec)(bitrateKbps)(channels)(sampleRate)(urlPath)(contentType)(noAuth)(useBasicAuth)(frameSize)

struct QnBounds
{
    // TODO: #dmishin move to signed integer type and refactor isNull method
    quint64 min;
    quint64 max;

    QnBounds() : min(0), max(0) {};
    bool isNull() const { return min == 0 && max == 0; };
};

#define QnBounds_Fields (min)(max)
QN_FUSION_DECLARE_FUNCTIONS(QnBounds, (json)(ubjson)(xml)(csv_record))
QN_FUSION_DECLARE_FUNCTIONS(TwoWayAudioParams, (json)(ubjson)(xml)(csv_record))

typedef std::vector<QnBounds> QnBoundsList;

struct UnauthorizedTimeoutLimits
{
    std::chrono::seconds min = std::chrono::minutes(1);
    // For many vendors, the unblocking period lasts 5 minutes, so the default value is set slightly higher.
    std::chrono::seconds max = std::chrono::minutes(6);
};

#define UnauthorizedTimeoutLimits_Fields (min)(max)
QN_FUSION_DECLARE_FUNCTIONS(UnauthorizedTimeoutLimits, (json) (ubjson) (xml) (csv_record))
