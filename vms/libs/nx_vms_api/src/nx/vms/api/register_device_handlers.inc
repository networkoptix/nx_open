// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

void registerDeviceBaseHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    nx::vms::network::AbstractServerConnector* serverConnector)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc POST /rest/v{1-}/devices/{id}/changePassword
     * Changes the password for the already existing User on the specified Device. This function
     * is allowed only for Devices with "SetUserPasswordCapability". Otherwise, it returns an error
     * in the JSON result.
     * %caption Change Device password
     * %ingroup Devices
     * %struct DevicePasswordRequest
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{1-}/devices/:id/changePassword", GlobalPermission::powerUser,
        std::make_unique<ChangeDevicePasswordHandler>(serverConnector));

    /**%apidoc GET /rest/v{1-2}/devices/{id}/footage
     * Retrieves the info for the recorded chunks for the Device specified by {id}.
     * %caption Get Device footage
     * %ingroup Device Media
     * %struct DeviceFootageRequest
     * %param:string id
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions View Archive on selected Device.
     * %return:{ServerTimePeriodList} List of recorded chunks.
     *     %param durationMs Duration in milliseconds. For the video chunk currently being
     *         recorded, `-1` is returned.
     *
     **%apidoc GET /rest/v{1-2}/devices/%2A/footage
     * Retrieves the info for the recorded chunks for all Devices in the Site.
     * %caption Get all Devices' footage
     * %ingroup Device Media
     * %struct DeviceFootageRequest
     * %param[opt] id
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions View Archive on some Devices.
     * %return:{std::map<nx::Uuid, ServerTimePeriodList>} List of recorded chunks.
     *     %param *[].durationMs Duration in milliseconds. For the video chunk currently being
     *         recorded, `-1` is returned.
     *
     **%apidoc GET /rest/v{3-}/devices/{id}/footage
     * Retrieves the info for the recorded chunks for the Device specified by {id}.
     * %caption Get Device footage
     * %ingroup Device Media
     * %struct DeviceFootageRequest
     * %param:string id
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions View Archive on selected Device.
     * %return:{ServerTimePeriodList} List of recorded chunks.
     *
     **%apidoc GET /rest/v{3-}/devices/%2A/footage
     * Retrieves the recorded chunks info for all Devices in the Site.
     * %caption Get all Devices' footage
     * %ingroup Device Media
     * %struct DeviceFootageRequest
     * %param[opt] id
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions View Archive on some Devices.
     * %return:{std::map<nx::Uuid, ServerTimePeriodList>} List of recorded chunks.
     */
    reg("rest/v{1-}/devices/:id/footage", GlobalPermission::none,
        std::make_unique<DeviceFootageHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{1-}/devices/{id}/status
     * Retrieves the Device Diagnosis information for the particular Device.
     * %caption Get Device Diagnosis
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Power User.
     * %return:{DeviceDiagnosis} Device Diagnosis information.
     *
     **%apidoc GET /rest/v{1-}/devices/%2A/status
     * Retrieves the Device Diagnosis information for all Devices.
     * %caption Get all Devices' Diagnoses
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Power User.
     * %return:{DeviceDiagnosisList} List of Device Diagnosis information.
     */
    reg("rest/v{1-}/devices/:id/status", GlobalPermission::powerUser,
        std::make_unique<DeviceDiagnoseHandler>(serverConnector, serverModule->videoCameraPool()));

    /**%apidoc GET /rest/v{3-}/devices/{id}/resourceData
     * Retrieves the data from Device configuration file (resource_data.json) which is currently in
     *     use, for the particular Device.
     * %caption Get Device's Resource data
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return:{DeviceResourceData} Device resource data information.
     *
     **%apidoc GET /rest/v{3-}/devices/%2A/resourceData
     * Retrieves the data from Device configuration file (resource_data.json) which is currently in
     *     use, for all Devices.
     * %caption Get all Devices' Resource data
     * %ingroup Devices
     * %param[ref] _filter,_format,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return:{DeviceResourceDataList} List of Device resource data.
     */
    reg("rest/v{3-}/devices/:id/resourceData", GlobalPermission::none,
        std::make_unique<DeviceResourceDataHandler>(serverModule->systemContext()));

    /**%apidoc POST /rest/v{2-}/devices/{id}/replace
     * Replaces the Device with another one. The replaced Device is removed completely, and its
     * settings and Archive are transferred to the Device it is replaced with.
     * %caption Replace Device
     * %ingroup Devices
     * %struct DeviceReplacementRequest
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{DeviceReplacementResponse} Device replacement report.
     *
     **%apidoc DELETE /rest/v{2-}/devices/{id}/replace
     * Deletes the Device replacement record.
     * %caption Undo Device replacement
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{2-}/devices/:id/replace", GlobalPermission::powerUser,
        std::make_unique<DeviceReplacementHandler>(serverModule));

    /**%apidoc POST /rest/v{2-}/devices/{id}/changeId
     * Changes the Device id to a new unique value. The next discovery will find the Device with
     * the new id.
     * %caption Change Device id
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:object
     *     %param:uuid newId Auto-generated id of the Device.
     *
     **%apidoc DELETE /rest/v{2-}/devices/{id}/changeId
     * Deletes the Device id change record.
     * %caption Undo Device id change
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{2-}/devices/:id/changeId", GlobalPermission::powerUser,
        std::make_unique<DeviceChangeIdHandler>(serverModule));

    /**%apidoc GET /rest/v{2-}/devices/{id}/advanced/%2A/manifest
     * Retrieves the Device Advanced Parameter manifest.
     * %caption Get Device Advanced manifest
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return:{QnCameraAdvancedParams} Device Advanced Manifest.
     *
     **%apidoc GET /rest/v{2-3}/devices/%2A/advanced/%2A/manifest
     * Retrieves the Advanced Parameter manifests of all Devices.
     * %caption Get Device Advanced manifests
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return:{std::map<nx::Uuid, QnCameraAdvancedParams>} Map with an Advanced Parameter manifest per Device.
     *
     **%apidoc GET /rest/v{4-}/devices/%2A/advanced/%2A/manifest
     * Retrieves the Advanced Parameter manifests of all Devices.
     * %caption Get Device Advanced manifests
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return:{std::map<nx::Uuid, QnCameraAdvancedParams>} Map with an Advanced Parameter manifest per Device.
     */
    reg("rest/v{2-}/devices/:id/advanced/*/manifest", GlobalPermission::none,
        std::make_unique<DeviceAdvancedManifestHandler>(serverConnector));

    /**%apidoc GET /rest/v{2-}/devices/{deviceId}/advanced/{id}
     * Retrieves the specified Advanced Parameter of the Device.
     * %caption Get Device Advanced Parameter
     * %ingroup Devices
     * %struct DeviceAdvancedFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return Device Advanced Parameter.
     *
     **%apidoc GET /rest/v{2-}/devices/{deviceId}/advanced
     * %caption Get Device Advanced Parameters
     * %ingroup Devices
     * %param:string deviceId Device id to get Advanced Parameters from.
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Read Device.
     * %return Device Advanced Parameters and their values.
     *
     **%apidoc:object PATCH /rest/v{2-}/devices/{deviceId}/advanced
     * Modifies a bunch of Device Advanced Parameters by passing a JSON object with the parameter ids
     * and values in the request body. The complete list of ids with their types is returned by the
     * "Get Device Advanced manifests" function.
     * %caption Modify Device Adv. Parameters
     * %ingroup Devices
     * %param:string deviceId Device id to modify Advanced Parameters for.
     * %param[ref] _local,_strict,_ticket
     * %permissions Edit Device.
     *
     **%apidoc:any PUT /rest/v{2-}/devices/{deviceId}/advanced/{id}
     * Replaces the single Device Advanced Parameter by passing a JSON value in the request body.
     * The complete list of Parameters with their types is returned by the "Get Device Advanced
     * Manifests" function.
     * %caption Replace Device Adv. Parameter
     * %ingroup Devices
     * %param:string deviceId Device id to modify an Advanced Parameter for.
     * %param:string id Parameter id to modify.
     * %param[ref] _local,_strict,_ticket
     * %permissions Edit Device.
     */
    reg("rest/v{2-}/devices/:deviceId/advanced/:id?", GlobalPermission::none,
        std::make_unique<DeviceAdvancedParametersHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/devices/{deviceId}/io
     * Returns current IO states from Device
     * %caption Get IO States
     * %ingroup Devices
     * %struct DeviceIoFilter
     * %param:string deviceId
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{DeviceIoState} IO states from Device.
     *
     **%apidoc GET /rest/v{4-}/devices/%2A/io
     * Returns current IO states from Devices
     * %caption Get IO States
     * %ingroup Devices
     * %struct DeviceIoFilter
     * %param[opt] deviceId
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{DeviceIoStateList} List of IO states.
     *
     **%apidoc PATCH /rest/v{4-}/devices/{deviceId}/io
     * Update current IO State of Device.
     * %caption Update IO State
     * %ingroup Devices
     * %struct DeviceIoUpdateRequest
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{4-}/devices/:deviceId/io",
        GlobalPermission::none,
        std::make_unique<DeviceIoHandler>(serverConnector));

    /**%apidoc POST /rest/v{3-}/devices/{deviceId}/ptz/move
     * Starts a continuous PTZ move.
     * %caption Start PTZ move
     * %ingroup Device PTZ
     * %struct PtzMovement
     *     %param[unused] type
     *     %param[unused] speed
     *     %param pan X-axis movement speed (-1.0 to 1.0).
     *     %param tilt Y-axis movement speed (-1.0 to 1.0).
     *     %param zoom Zoom in/out speed (-1.0 to 1.0).
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     *
     **%apidoc DELETE /rest/v{3-}/devices/{deviceId}/ptz/move
     * Stops a continuous PTZ move.
     * %caption Stop PTZ move
     * %ingroup Device PTZ
     * %struct PtzPositionFilter
     *     %param[unused] type
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/move",
        GlobalPermission::none,
        std::make_unique<ptz::MoveHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{deviceId}/ptz/position
     * Retrieves the Device current position. Returns x, y, and z in the range defined by the
     * Device.
     * %caption Get Device PTZ position
     * %ingroup Device PTZ
     * %struct PtzPositionFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{PtzPosition} Device position
     *     %param[unused] deviceId
     *     %param[unused] type
     *     %param[unused] speed
     *
     **%apidoc POST /rest/v{3-}/devices/{deviceId}/ptz/position
     * Moves the Device to an absolute position. When the Device reports its PTZ capabilities to
     * the Server during initialization, the range of position parameters is sent to the Server.
     * PTZ capabilities can not be read via API.
     * %caption Move to absolute PTZ position
     * %ingroup Device PTZ
     * %struct PtzPosition
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{PtzPosition} Device position
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/position",
        GlobalPermission::none,
        std::make_unique<ptz::PositionHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{deviceId}/ptz/limits
     * Retrieves the Device PTZ limitations.
     * %caption Get Device PTZ limits
     * %ingroup Device PTZ
     * %struct PtzPositionFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{PtzPositionLimits} Device position limits
     *     %param[unused] deviceId
     *     %param[unused] type
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/limits",
        GlobalPermission::none,
        std::make_unique<ptz::PositionLimitsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{deviceId}/ptz/presets
     * Retrieves the PTZ preset list.
     * %caption Get PTZ presets
     * %ingroup Device PTZ
     * %struct PtzPresetFilter
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{PtzPresetList} Preset list
     *
     **%apidoc POST /rest/v{3-}/devices/{deviceId}/ptz/presets
     * Creates a new PTZ preset.
     * %caption Create PTZ preset
     * %ingroup Device PTZ
     * %struct PtzPreset
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{PtzPreset}
     *
     **%apidoc PATCH /rest/v{3-}/devices/{deviceId}/ptz/presets/{id}
     * Updates the display name of the specified PTZ preset.
     * %caption Update PTZ preset name
     * %ingroup Device PTZ
     * %struct PtzPreset
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{PtzPreset}
     *
     **%apidoc DELETE /rest/v{3-}/devices/{deviceId}/ptz/presets/{id}
     * Deletes the specified PTZ preset.
     * %caption Delete PTZ preset
     * %ingroup Device PTZ
     * %struct PtzPresetFilter
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/presets/:id?",
        GlobalPermission::none,
        std::make_unique<ptz::PresetHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{3-}/devices/{deviceId}/ptz/presets/{id}/activate
     * Activates the specified PTZ preset.
     * %caption Activate PTZ preset
     * %ingroup Device PTZ
     * %struct PtzPresetActivation
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/presets/:id/activate",
        GlobalPermission::none,
        std::make_unique<ptz::PresetActivationHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{deviceId}/ptz/tours
     * Retrieves the PTZ Tour list.
     * %caption Get PTZ Tours
     * %ingroup Device PTZ
     * %param:string deviceId Device id to get PTZ Tours from.
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{PtzTourList} Tour list
     *
     **%apidoc POST /rest/v{3-}/devices/{deviceId}/ptz/tours
     * Creates a new PTZ Tour.
     * %caption Create PTZ Tour
     * %ingroup Device PTZ
     * %struct PtzTour
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{PtzTour}
     *
     **%apidoc DELETE /rest/v{3-}/devices/{deviceId}/ptz/tours/{id}
     * Deletes the specified PTZ Tour.
     * %caption Delete PTZ Tour
     * %ingroup Device PTZ
     * %struct PtzPresetFilter
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/tours/:id?",
        GlobalPermission::none,
        std::make_unique<ptz::TourHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{deviceId}/ptz/tours/%2A/active
     * Retrieves the active PTZ Tour.
     * %caption Get active PTZ Tour
     * %ingroup Device PTZ
     * %struct PtzPresetFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_ticket,_with
     * %param[ref] _local,_strict
     * %permissions Depends on Resource access rights.
     * %return:{PtzTour} Active Tour
     *
     **%apidoc POST /rest/v{3-}/devices/{deviceId}/ptz/tours/{id}/active
     * Activates the specified PTZ Tour.
     * %caption Activate PTZ Tour
     * %ingroup Device PTZ
     * %struct PtzPresetFilter
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{3-}/devices/:deviceId/ptz/tours/:id/active",
        GlobalPermission::none,
        std::make_unique<ptz::TourActivationHandler>(serverModule, serverConnector));
}
