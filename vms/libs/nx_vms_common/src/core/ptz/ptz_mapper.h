// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <utils/math/space_mapper.h>

#include <nx/vms/common/ptz/vector.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"

class QnPtzMapperPrivate;

class NX_VMS_COMMON_API QnPtzMapper
{
    using Vector = nx::vms::common::ptz::Vector;

public:
    /**
     * Note that in both mappers source is device coordinates and target is logical coordinates.
     *
     * \param inputMapper               Mapper to use when sending data to the camera.
     * \param outputMapper              Mapper to use when receiving data from the camera.
     */
    QnPtzMapper(
        const QnSpaceMapperPtr<Vector> &inputMapper,
        const QnSpaceMapperPtr<Vector> &outputMapper);

    Vector logicalToDevice(const Vector &position) const
    {
        return m_inputMapper->targetToSource(position);
    }

    Vector deviceToLogical(const Vector& position) const
    {
        return m_outputMapper->sourceToTarget(position);
    }

    const QnPtzLimits& logicalLimits() const
    {
        return m_logicalLimits;
    }

private:
    QnSpaceMapperPtr<Vector> m_inputMapper;
    QnSpaceMapperPtr<Vector> m_outputMapper;
    QnPtzLimits m_logicalLimits;
};

#endif // QN_PTZ_MAPPER_H
