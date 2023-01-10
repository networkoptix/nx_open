// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

class QnResourceVideoLayout;
using QnResourceVideoLayoutPtr = std::shared_ptr<QnResourceVideoLayout>;
using QnConstResourceVideoLayoutPtr = std::shared_ptr<const QnResourceVideoLayout>;

class AudioLayout;
using AudioLayoutPtr = std::shared_ptr<AudioLayout>;
using AudioLayoutConstPtr = std::shared_ptr<const AudioLayout>;

class QnCustomResourceVideoLayout;
using QnCustomResourceVideoLayoutPtr = std::shared_ptr<QnCustomResourceVideoLayout>;
using QnConstCustomResourceVideoLayoutPtr = std::shared_ptr<const QnCustomResourceVideoLayout>;
