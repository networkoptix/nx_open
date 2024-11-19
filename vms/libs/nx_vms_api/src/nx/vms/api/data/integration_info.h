// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api {

struct NX_VMS_API IntegrationInfoRequest
{
    /**%apidoc:stringArray
     * Flexible id of an Integration or a set of flexible ids. Can be obtained from "id" field via
     *     `GET /rest/v{3-}/integrations`
     */
    nx::vms::api::json::ValueOrArray<QString> id;
};
#define IntegrationInfoRequest_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(IntegrationInfoRequest, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(IntegrationInfoRequest, IntegrationInfoRequest_Fields)

/**%apidoc
 * Information about bound resources. For Analytics Plugins contains information about the
 * resources bound to its Engine. For Device Plugins the information is related to the resources
 * bound to the Plugin itself.
 */
struct NX_VMS_API IntegrationResourceBindingInfo
{
    /**%apidoc
     * Id of the Engine in the case of Analytics Integrations, an empty string for Device Integrations.
     */
    QString id;

    /**%apidoc
     * Name of the Engine (for Analytics Integrations) or the Integration (for Device Integrations).
     */
    QString name;

    /**%apidoc
     * For Analytics Integrations - number of resources on which the Engine is enabled automatically or
     * by the User. In the case of Device Integrations - the number of resources that are produced by
     * the Integration.
     */
    int boundResourceCount = 0;

    /**%apidoc
     * For Analytics Integrations - number of resources on which the Engine is enabled
     * automatically or by the User and that are currently online (thus a Device Agent is created
     * for them). In the case of Device Integrations - the number of Resources that are produced by
     * the Integration and are online.
     */
    int onlineBoundResourceCount = 0;
};
#define IntegrationResourceBindingInfo_Fields \
    (id) \
    (name) \
    (boundResourceCount) \
    (onlineBoundResourceCount)
QN_FUSION_DECLARE_FUNCTIONS(IntegrationResourceBindingInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(IntegrationResourceBindingInfo, IntegrationResourceBindingInfo_Fields)

/**%apidoc
 * Information about a Server Integration that resides in a plugin (dynamic library). If the
 * Integration object was not created because of issues with the dynamic library, this structure
 * keeps information about these issues. If there is more than one Integration object created by
 * the same dynamic library, each will have its own instance of this structure.
 */
struct NX_VMS_API IntegrationInfo
{
    // TODO: Group fields specific to plugin types.
    /**%apidoc
     * Name of the Integration from its manifest.
     */
    QString name;

    /**%apidoc
     * Description of the Integration from its manifest.
     */
    QString description;

    /**%apidoc
     * Vendor of the Integration from its manifest.
     */
    QString vendor;

    /**%apidoc
     * Version of the Integration from its manifest.
     */
    QString version;

    /**%apidoc
     * Array with information about bound Resources. For Device Integrations contains zero (if the
     * plugin is not loaded) or one item. For Analytics Integrations, the number of items is equal
     * to the number of Engines of the Integration.
     */
    std::vector<IntegrationResourceBindingInfo> resourceBindingInfo;

    /**%apidoc
     * Plugin-dependent properties, is subject to change in the future.
     */
    std::map<QString, QJsonValue> properties;
};
#define IntegrationInfo_Fields \
    (name) \
    (description) \
    (vendor) \
    (version) \
    (resourceBindingInfo) \
    (properties)
QN_FUSION_DECLARE_FUNCTIONS(IntegrationInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(IntegrationInfo, IntegrationInfo_Fields)

} // namespace nx::vms::api
