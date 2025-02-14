// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "common.h"

namespace nx::vms::api::rules {

struct NX_VMS_API GenericEventData
{
    /**%apidoc[opt] Event date and time, as a string containing time in milliseconds since epoch,
     * or a local time formatted like
     * <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     * the format is auto-detected. If "timestamp" is absent or "now", the current Server date and
     * time is used.
     */
    QString timestamp;

    /**%apidoc[opt] Generic Events can be used either with "prolonged" actions like
     * "Device recording", or instant actions like "Send email". This parameter should be specified
     * in case "prolonged" actions are going to be used with Generic Events. The Event is
     * considered instant in case of parameter absence.
     * %example instant
     */
    State state = State::instant;

    /**%apidoc[opt] Name of the Device which has triggered the Event. It can be used
     * in a filter in VMS Rules to assign different actions to different Devices. Also, the
     * user could see this name in the notification panel.
     * %example POS terminal 5
     */
    QString source;

    /**%apidoc[opt] Short Event description. It can be used in a filter in VMS Rules to assign
     * actions depending on this text.
     */
    QString caption;

    /**%apidoc[opt] Long Event description. It can be used as a filter in VMS Rules to assign
     * actions depending on this text.
     */
    QString description;

    /**%apidoc[opt] Specifies the list of Devices which are linked to the Event (e.g.
     * the Event will appear on their timelines), in the form of a list of Device ids (can
     * be obtained from "id" field via /rest/v{1-}/devices).
     * %example []
     */
    std::vector<QString> deviceIds;
};

#define GenericEventData_Fields (timestamp)(state)(source)(caption)(description)(deviceIds)
QN_FUSION_DECLARE_FUNCTIONS(GenericEventData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(GenericEventData, GenericEventData_Fields)

} // namespace nx::vms::rules
