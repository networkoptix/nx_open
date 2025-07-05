// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls

AdaptiveTransmission
{
    id: transmission

    maximumFactor: 10.0
    factorDelta: 0.5
    decayDelayMs: 200
    fullDecayDurationMs: 500

    // Pixel deltas from high-precision touchpads are processed as is;
    // angle deltas from low-precision touchpads & mouses are processed with acceleration applied.
    function pixelDelta(wheelEvent, pixelsPer15DegreeStep)
    {
        return wheelEvent.pixelDelta.y
            || transmission.transform(
                angleDeltaToPixelDelta(wheelEvent.angleDelta.y, pixelsPer15DegreeStep))
    }

    function angleDeltaToPixelDelta(angleDegrees, pixelsPer15DegreeStep)
    {
        // Standard mouse values.
        const kUnitsPerDegree = 8.0
        const kDegreesPerStep = 15.0
        const steps = angleDegrees / (kUnitsPerDegree * kDegreesPerStep)
        return steps * pixelsPer15DegreeStep
    }
}
