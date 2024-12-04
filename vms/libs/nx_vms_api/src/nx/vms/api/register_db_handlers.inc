// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

{
    /**%apidoc GET /rest/v{1-3}/servers
     * Retrieves all Server records stored in the Site.
     * %caption Get Servers
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ServerModelV1List} List of all Server records.
     *     %param[unused] authKey
     *
     **%apidoc POST /rest/v{1-3}/servers
     * Creates a record in the Site for the new Server.
     * %caption Create Server
     * %ingroup Servers
     * %struct ServerModelV1
     * %param[unused] id
     * %param[unused] storages
     * %param:{std::variant<int, QString>} backupBitrateBytesPerSecond[].value
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{ServerModelV1} Server record that was created.
     *     %param[unused] authKey
     *
     **%apidoc GET /rest/v{1-3}/servers/{id}
     * Retrieves the specified Server record stored in the Site.
     * %caption Get Server
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ServerModelV1} Server record.
     *     %param[unused] authKey
     *
     **%apidoc PUT /rest/v{1-3}/servers/{id}
     * Replaces all fields of the specified Server record stored in the Site.
     * Creation of a new Server or modification of some settings requires an `Administrator with a
     * fresh session` permissions.
     * %caption Replace Server
     * %ingroup Servers
     * %struct ServerModelV1
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:{std::variant<int, QString>} backupBitrateBytesPerSecond[].value
     * %param[unused] storages
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{ServerModelV1} Server record.
     *     %param[unused] authKey
     *
     **%apidoc PATCH /rest/v{1-3}/servers/{id}
     * Modifies certain fields of the specified Server record stored in the Site.
     * Modification of some settings requires an `Administrator with a fresh session` permissions.
     * %caption Modify Server
     * %ingroup Servers
     * %struct[opt] ServerModelV1
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:{std::variant<int, QString>} backupBitrateBytesPerSecond[].value
     * %param[unused] storages
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{ServerModelV1} Server record.
     *     %param[unused] authKey
     *
     **%apidoc DELETE /rest/v{1-3}/servers/{id}
     * Deletes the specified Server record from the Site.
     * %caption Delete Server
     * %ingroup Servers
     * %param:uuid id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`.
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{1-3}/servers/:id?", GlobalPermission::none, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<nx::vms::server::ServersHandler<nx::vms::api::ServerModelV1, QueryProcessor>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v{4-}/servers
     * Retrieves all Server records stored in the Site.
     * %caption Get Servers
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ServerModelV4List} List of all Server records.
     *     %param[unused] authKey
     *
     **%apidoc POST /rest/v{4-}/servers
     * Creates a record in the Site for the new Server.
     * %caption Create Server
     * %ingroup Servers
     * %struct ServerModelV4
     * %param[unused] id
     * %param[unused] storages
     * %param:{std::variant<int, QString>} backupBitrateBytesPerSecond[].value
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{ServerModelV4} Server record that was created.
     *     %param[unused] authKey
     *
     **%apidoc GET /rest/v{4-}/servers/{id}
     * Retrieves the specified Server record stored in the Site.
     * %caption Get Server
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ServerModelV4} Server record.
     *     %param[unused] authKey
     *
     **%apidoc PUT /rest/v{4-}/servers/{id}
     * Replaces all fields of the specified Server record stored in the Site.
     * Creation of a new Server or modification of some settings requires an `Administrator with a
     * fresh session` permissions.
     * %caption Replace Server
     * %ingroup Servers
     * %struct ServerModelV4
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:{std::variant<int, QString>} backupBitrateBytesPerSecond[].value
     * %param[unused] storages
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{ServerModelV4} Server record.
     *     %param[unused] authKey
     *
     **%apidoc PATCH /rest/v{4-}/servers/{id}
     * Modifies certain fields of the specified Server record stored in the Site.
     * Modification of some settings requires an `Administrator with a fresh session` permissions.
     * %caption Modify Server
     * %ingroup Servers
     * %struct[opt] ServerModelV4
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:{std::variant<int, QString>} backupBitrateBytesPerSecond[].value
     * %param[unused] storages
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{ServerModelV4} Server record.
     *     %param[unused] authKey
     *
     **%apidoc DELETE /rest/v{4-}/servers/{id}
     * Deletes the specified Server record from the Site.
     * %caption Delete Server
     * %ingroup Servers
     * %param:uuid id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`.
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{4-}/servers/:id?", GlobalPermission::none, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<nx::vms::server::ServersHandler<nx::vms::api::ServerModelV4, QueryProcessor>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v{1-}/servers/{serverId}/storages
     * Retrieves all Storage records stored in the Site.
     * %caption Get Storages
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server, or be omitted by
     *     using '*' placeholder to use all Servers.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{StorageModelList} List of Storage records.
     *
     **%apidoc POST /rest/v{1-}/servers/{serverId}/storages
     * Creates a record in the Site for the new Storage.
     * %caption Create Storage
     * %ingroup Server Data
     * %struct StorageModel
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
      *     %example this
     * %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{StorageModel} Storage record that was created.
     *
     **%apidoc GET /rest/v{1-}/servers/{serverId}/storages/{id}
     * Retrieves the specified Storage record stored in the Site.
     * %caption Get Storage
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server, or be omitted by
     *     using '*' placeholder.
     *     %example this
     * %param:uuid id Storage id (can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers/{serverId}/storages`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{StorageModel} Storage record.
     *
     **%apidoc PUT /rest/v{1-}/servers/{serverId}/storages/{id}
     * Replaces all fields of the specified Storage record stored in the Site.
     * %caption Replace Storage
     * %ingroup Server Data
     * %struct StorageModel
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
      *     %example this
     * %param:uuid id Storage id (can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers/{serverId}/storages`).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{StorageModel} Storage record.
     *
     **%apidoc PATCH /rest/v{1-}/servers/{serverId}/storages/{id}
     * Modifies certain fields of the specified Storage record stored in the Site.
     * %caption Modify Storage
     * %ingroup Server Data
     * %struct[opt] StorageModel
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
      *     %example this
     * %param:uuid id Storage id (can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers/{serverId}/storages`).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{StorageModel} Storage record.
     *
     **%apidoc DELETE /rest/v{1-}/servers/{serverId}/storages/{id}
     * Deletes the specified Storage record from the Site.
     * %caption Delete Storage
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:uuid id Storage id (can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers/{serverId}/storages`).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{1-}/servers/:serverId/storages/:id?", GlobalPermission::none,
        std::make_unique<nx::vms::server::StoragesHandler<QueryProcessor>>(
            queryProcessor, storageAccess, systemContext->resourcePool(), systemContext->peerId()));

    /**%apidoc GET /rest/v{1-2}/devices
     * Retrieves all Device records stored in the Site.
     * %caption Get Devices
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<DeviceModelV1>} List of all Device records.
     *
     **%apidoc POST /rest/v{1-2}/devices
     * Creates a record in the Site for the new Device. This method does not check for the Device
     * availability; use <code>POST /rest/v{1-}/devices/&ast;/searches</code> with `mode` equal to
     * `addFoundDevices` whenever possible instead.
     * %caption Create Device
     * %ingroup Devices
     * %struct DeviceModelV1
     *     %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV1} Device record that was created.
     *
     **%apidoc GET /rest/v{1-2}/devices/{id}
     * Retrieves the specified Device record stored in the Site.
     * %caption Get Device
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV1} Device record.
     *
     **%apidoc PUT /rest/v{1-2}/devices/{id}
     * Replaces all fields of the specified Device record stored in the Site.
     * %caption Replace Device
     * %ingroup Devices
     * %struct DeviceModelV1
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV1} Device record.
     *
     **%apidoc PATCH /rest/v{1-2}/devices/{id}
     * Modifies certain fields of the specified Device record stored in the Site.
     * %caption Modify Device
     * %ingroup Devices
     * %struct [opt] DeviceModelV1
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV1} Device record.
     *
     **%apidoc DELETE /rest/v{1-2}/devices/{id}
     * Deletes the specified Device record from the Site.
     * %caption Delete Device
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{1-2}/devices/:id?",
        GlobalPermission::none,
        std::make_unique<
            nx::vms::server::DevicesHandler<QueryProcessor, nx::vms::api::DeviceModelV1>>(
                queryProcessor, systemContext));

    /**%apidoc GET /rest/v3/devices
     * Retrieves all Device records stored in the Site.
     * %caption Get Devices
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<DeviceModelV3>} List of all Device records.
     *
     **%apidoc POST /rest/v3/devices
     * Creates a record in the Site for the new Device. This method does not check for the Device
     * availability; use <code>POST /rest/v{1-}/devices/&ast;/searches</code> with `mode` equal to
     * `addFoundDevices` whenever possible instead.
     * %caption Create Device
     * %ingroup Devices
     * %struct DeviceModelV3
     *     %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3} Device record that was created.
     *
     **%apidoc GET /rest/v3/devices/{id}
     * Retrieves the specified Device record stored in the Site.
     * %caption Get Device
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3} Device record.
     *
     **%apidoc PUT /rest/v3/devices/{id}
     * Replaces all fields of the specified Device record stored in the Site.
     * %caption Replace Device
     * %ingroup Devices
     * %struct DeviceModelV3
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3} Device record.
     *
     **%apidoc PATCH /rest/v3/devices/{id}
     * Modifies certain fields of the specified Device record stored in the Site.
     * %caption Modify Device
     * %ingroup Devices
     * %struct [opt] DeviceModelV3
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3} Device record.
     *
     **%apidoc DELETE /rest/v3/devices/{id}
     * Deletes the specified Device record from the Site.
     * %caption Delete Device
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v3/devices/:id?",
        GlobalPermission::none,
        std::make_unique<
            nx::vms::server::DevicesHandler<QueryProcessor, nx::vms::api::DeviceModelV3>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v{4-}/devices
     * Retrieves all Device records stored in the Site.
     * %caption Get Devices
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<DeviceModelV4>} List of all Device records.
     *
     **%apidoc POST /rest/v{4-}/devices
     * Creates a record in the Site for the new Device. This method does not check for the Device
     * availability; use <code>POST /rest/v{1-}/devices/&ast;/searches</code> with `mode` equal to
     * `addFoundDevices` whenever possible instead.
     * %caption Create Device
     * %ingroup Devices
     * %struct DeviceModelV4
     *     %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4} Device record that was created.
     *
     **%apidoc GET /rest/v{4-}/devices/{id}
     * Retrieves the specified Device record stored in the Site.
     * %caption Get Device
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4} Device record.
     *
     **%apidoc PUT /rest/v{4-}/devices/{id}
     * Replaces all fields of the specified Device record stored in the Site.
     * %caption Replace Device
     * %ingroup Devices
     * %struct DeviceModelV4
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4} Device record.
     *
     **%apidoc PATCH /rest/v{4-}/devices/{id}
     * Modifies certain fields of the specified Device record stored in the Site.
     * %caption Modify Device
     * %ingroup Devices
     * %struct [opt] DeviceModelV4
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4} Device record.
     *
     **%apidoc DELETE /rest/v{4-}/devices/{id}
     * Deletes the specified Device record from the Site.
     * %caption Delete Device
     * %ingroup Devices
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     *     `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{4-}/devices/:id?",
        GlobalPermission::none,
        std::make_unique<
            nx::vms::server::DevicesHandler<QueryProcessor, nx::vms::api::DeviceModelV4>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v{1-}/devices/%2A/types
     * Retrieves the list of the supported Device Types.
     * %caption Get Device Types
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{DeviceTypeModelList} Device type record.
     */
    reg("rest/v{1-}/devices/*/types", GlobalPermission::none,
        std::make_unique<nx::vms::server::DeviceTypesHandler>());

    // TODO: (?) PUT
    /**%apidoc POST /rest/v3/devices/%2A/virtual
     * Creates a record in the Site for the new virtual Device.
     * %caption Create Virtual Device
     * %ingroup Virtual Devices
     * %struct[opt] VirtualDeviceCreate
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3} Device record that was created.
     *
     * %apidoc GET /rest/v3/devices/%2A/virtual
     * Retrieves a list of virtual Devices.
     * %caption Get Virtual Devices
     * %ingroup Virtual Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<DeviceModelV3>}
     *
     * %apidoc GET /rest/v3/devices/{id}/virtual
     * Retrieves a virtual device by id.
     * %caption Get Virtual Device
     * %ingroup Virtual Devices
     * %param:string id Virtual device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v3/devices/&ast;/virtual`) or MAC address.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3}
     *
     * %apidoc PATCH /rest/v3/devices/{id}/virtual
     * Modifies a virtual Device by id.
     * %caption Modify Virtual Device
     * %ingroup Virtual Devices
     * %struct[opt] DeviceModelV3
     * %param:string id Virtual device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v3/devices/&ast;/virtual`) or MAC address.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV3} Device record that was created.
     *
     * %apidoc DELETE /rest/v3/devices/{id}/virtual
     * Deletes a virtual Device by id.
     * %caption Delete Virtual Device
     * %ingroup Virtual Devices
     * %param[ref] _strict,_ticket
     * %param:string id Virtual device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v3/devices/&ast;/virtual`) or MAC address.
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v3/devices/:id/virtual", GlobalPermission::none,
        std::make_unique<
            nx::vms::server::VirtualDevicesHandler<QueryProcessor, nx::vms::api::DeviceModelV3>>(
            queryProcessor, systemContext));

    /**%apidoc POST /rest/v{4-}/devices/%2A/virtual
     * Creates a record in the Site for the new virtual Device.
     * %caption Create Virtual Device
     * %ingroup Virtual Devices
     * %struct[opt] VirtualDeviceCreate
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4} Device record that was created.
     *
     * %apidoc GET /rest/v{4-}/devices/%2A/virtual
     * Retrieves a list of virtual Devices.
     * %caption Get Virtual Devices
     * %ingroup Virtual Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<DeviceModelV4>}
     *
     * %apidoc GET /rest/v{4-}/devices/{id}/virtual
     * Retrieves a virtual device by id.
     * %caption Get Virtual Device
     * %ingroup Virtual Devices
     * %param:string id Virtual device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{4-}/devices/&ast;/virtual`) or MAC address.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4}
     *
     * %apidoc PATCH /rest/v{4-}/devices/{id}/virtual
     * Modifies a virtual Device by id.
     * %caption Modify Virtual Device
     * %ingroup Virtual Devices
     * %struct[opt] DeviceModelV4
     * %param:string id Virtual device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{4-}/devices/&ast;/virtual`) or MAC address.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{DeviceModelV4} Device record that was created.
     *
     * %apidoc DELETE /rest/v{4-}/devices/{id}/virtual
     * Deletes a virtual Device by id.
     * %caption Delete Virtual Device
     * %ingroup Virtual Devices
     * %param[ref] _strict,_ticket
     * %param:string id Virtual device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{4-}/devices/&ast;/virtual`) or MAC address.
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{4-}/devices/:id/virtual", GlobalPermission::none,
        std::make_unique<
            nx::vms::server::VirtualDevicesHandler<QueryProcessor, nx::vms::api::DeviceModelV4>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v{1-2}/users
     * Retrieves all User records stored in the Site.
     * %caption Get Users
     * %ingroup Users
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{UserModelV1List} List of all User records.
     *     %param[unused] password
     *
     **%apidoc POST /rest/v{1-2}/users
     * Creates a record in the Site for the new User.
     * %caption Create User
     * %ingroup Users
     * %struct UserModelV1
     * %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserModelV1} User record that was created.
     *     %param[unused] password
     *
     **%apidoc GET /rest/v{1-2}/users/{id}
     * Retrieves the specified User record stored in the Site.
     * %caption Get User
     * %ingroup Users
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User or the user themselves.
     * %return:{UserModelV1} User record.
     *     %param[unused] password
     *
     **%apidoc PUT /rest/v{1-2}/users/{id}
     * Replaces all fields of the specified User record stored in the Site.
     * %caption Replace User
     * %ingroup Users
     * %struct UserModelV1
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[unused] isOwner
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     * %return:{UserModelV1} User record.
     *     %param[unused] password
     *
     **%apidoc PATCH /rest/v{1-2}/users/{id}
     * Modifies certain fields of the specified User record stored in the Site.
     * %caption Modify User
     * %ingroup Users
     * %struct[opt] UserModelV1
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[unused] type
     * %param[unused] isOwner
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     * %return:{UserModelV1} User record.
     *     %param[unused] password
     *
     **%apidoc DELETE /rest/v{1-2}/users/{id}
     * Deletes the specified User record from the Site.
     * %caption Delete User
     * %ingroup Users
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     */
    reg("rest/v{1-2}/users/:id?", GlobalPermission::none, GlobalPermission::requireFreshSession,
        std::make_unique<nx::vms::server::UsersHandler<nx::vms::api::UserModelV1, QueryProcessor>>(
            queryProcessor, systemContext, cloudManagerGroup));

    /**%apidoc GET /rest/v3/users
     * Retrieves all User records stored in the Site.
     * %caption Get Users
     * %ingroup Users
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{UserModelV3List} List of all User records.
     *     %param[unused] password
     *     %param[unused] settings
     *
     **%apidoc POST /rest/v3/users
     * Creates a record in the Site for the new User.
     * %caption Create User
     * %ingroup Users
     * %struct UserModelV3
     * %param[unused] id
     * %param[unused] externalId
     * %param[unused] settings
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserModelV3} User record that was created.
     *     %param[unused] password
     *     %param[unused] settings
     *
     **%apidoc GET /rest/v3/users/{id}
     * Retrieves the specified User record stored in the Site.
     * %caption Get User
     * %ingroup Users
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User or the user themselves.
     * %return:{UserModelV3} User record.
     *     %param[unused] password
     *     %param[unused] settings
     *
     **%apidoc PUT /rest/v3/users/{id}
     * Replaces all fields of the specified User record stored in the Site.
     * %caption Replace User
     * %ingroup Users
     * %struct UserModelV3
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[unused] externalId
     * %param[unused] isOwner
     * %param[unused] settings
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     * %return:{UserModelV3} User record.
     *     %param[unused] password
     *     %param[unused] settings
     *
     **%apidoc PATCH /rest/v3/users/{id}
     * Modifies certain fields of the specified User record stored in the Site.
     * %caption Modify User
     * %ingroup Users
     * %struct[opt] UserModelV3
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[unused] externalId
     * %param[unused] type
     * %param[unused] isOwner
     * %param[unused] settings
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     * %return:{UserModelV3} User record.
     *     %param[unused] password
     *     %param[unused] settings
     *
     **%apidoc DELETE /rest/v3/users/{id}
     * Deletes the specified User record from the Site.
     * %caption Delete User
     * %ingroup Users
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     *
     **%apidoc GET /rest/v{4-}/users
     * Retrieves all User records stored in the Site.
     * %caption Get Users
     * %ingroup Users
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{UserModelV3List} List of all User records.
     *     %param[unused] password
     *
     **%apidoc POST /rest/v{4-}/users
     * Creates a record in the Site for the new User.
     * %caption Create User
     * %ingroup Users
     * %struct UserModelV3
     * %param[unused] id
     * %param[unused] externalId
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserModelV3} User record that was created.
     *     %param[unused] password
     *
     **%apidoc GET /rest/v{4-}/users/{id}
     * Retrieves the specified User record stored in the Site.
     * %caption Get User
     * %ingroup Users
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User or the user themselves.
     * %return:{UserModelV3} User record.
     *     %param[unused] password
     *
     **%apidoc PUT /rest/v{4-}/users/{id}
     * Replaces all fields of the specified User record stored in the Site.
     * %caption Replace User
     * %ingroup Users
     * %struct UserModelV3
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[unused] externalId
     * %param[unused] isOwner
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     * %return:{UserModelV3} User record.
     *     %param[unused] password
     *
     **%apidoc PATCH /rest/v{4-}/users/{id}
     * Modifies certain fields of the specified User record stored in the Site.
     * %caption Modify User
     * %ingroup Users
     * %struct[opt] UserModelV3
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[unused] externalId
     * %param[unused] type
     * %param[unused] isOwner
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     * %return:{UserModelV3} User record.
     *     %param[unused] password
     *
     **%apidoc DELETE /rest/v{4-}/users/{id}
     * Deletes the specified User record from the Site.
     * %caption Delete User
     * %ingroup Users
     * %param:string id User id or url-encoded name (can be obtained from "id" or "name" fields via
     *     `GET /rest/v{1-}/users`).
     *     %example admin
     * %param[ref] _strict,_ticket
     * %permissions Power User or the user themselves with a fresh session.
     */
    reg("rest/v{3-}/users/:id?", GlobalPermission::none, GlobalPermission::requireFreshSession,
        std::make_unique<nx::vms::server::UsersHandler<nx::vms::api::UserModelV3, QueryProcessor>>(
            queryProcessor, systemContext, cloudManagerGroup));

    /**%apidoc GET /rest/v{1-}/storedFiles
     * Retrieves all Stored File records stored in the Site.
     * %caption Get Stored Files
     * %ingroup Stored Files
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{StoredFileDataList} List of all Stored File records.
     *
     **%apidoc GET /rest/v{1-}/storedFiles/{path}
     * Retrieves the specified Stored File record stored in the Site.
     * %caption Get Stored File
     * %ingroup Stored Files
     * %param:string path Stored File path (can be obtained from `path` field via
     *     `GET /rest/v{1-}/storedFiles`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{StoredFileData} Stored File record.
     *
     **%apidoc PUT /rest/v{1-}/storedFiles/{path}
     * Creates or replaces a record in the Site about the new or already existing Stored File.
     * %caption Save Stored File
     * %ingroup Stored Files
     * %struct StoredFileData
     * %param:string path Stored File path (can be obtained from `path` field via
     *     `GET /rest/v{1-}/storedFiles`).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{StoredFileData} Stored File record.
     *
     **%apidoc DELETE /rest/v{1-}/storedFiles/{path}
     * Deletes the specified Stored File record from the Site.
     * %caption Delete Stored File
     * %ingroup Stored Files
     * %param:string path Stored File path (can be obtained from `path` field via
     *     `GET /rest/v{1-}/storedFiles`).
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{1-}/storedFiles/:path+", GlobalPermission::none,
        std::make_unique<nx::vms::server::StoredFilesHandler<QueryProcessor>>(queryProcessor));

    /**%apidoc GET /rest/v{1-}/licenses
     * Retrieves all License records stored in the Site.
     * %caption Get Licenses
     * %ingroup Licenses
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{LicenseDataList} List of all License records.
     *
     **%apidoc GET /rest/v{1-}/licenses/{key}
     * Retrieves the specified License record stored in the Site.
     * %caption Get License
     * %ingroup Licenses
     * %param:string key License key (existing key can be obtained from "key" field via
     *     `GET /rest/v{1-}/licenses`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{LicenseData} License record.
     *
     **%apidoc PUT /rest/v{1-}/licenses/{key}
     * Creates a record about the License and activates it with the data provided by the VMS
     * license server if `licenseBlock` is unspecified. If `licenseBlock` is specified, creates a
     * License with the data provided.
     * %caption Create License
     * %ingroup Licenses
     * %struct[opt] LicenseData
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{LicenseData} License record.
     *
     **%apidoc DELETE /rest/v{1-}/licenses/{key}
     * Deletes the specified License record from the Site.
     * %caption Delete License
     * %ingroup Licenses
     * %param:string key License key (existing key can be obtained from "key" field via
     *     `GET /rest/v{1-}/licenses`).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{1-}/licenses/:key?", GlobalPermission::powerUser,
        std::make_unique<nx::vms::server::LicensesHandler<QueryProcessor>>(
            systemContext, queryProcessor, std::move(licenseRequest)));

    /**%apidoc GET /rest/v{1-2}/userRoles
     * Retrieves all User Role records stored in the Site.
     * %caption Get User Roles
     * %ingroup User Roles
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{UserRoleModelList} List of all User Role records.
     *
     **%apidoc POST /rest/v{1-2}/userRoles
     * Creates a record in the Site for the new User Role.
     * %caption Create User Role
     * %ingroup User Roles
     * %struct UserRoleModel
     * %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserRoleModel} User Role record that was created.
     *
     **%apidoc GET /rest/v{1-2}/userRoles/{id}
     * Retrieves the specified User Role record stored in the Site.
     * %caption Get User Role
     * %ingroup User Roles
     * %param:uuid id User Role id (can be obtained from "id" field via
     *     `GET /rest/v{1-2}/userRoles`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{UserRoleModel} User Role record.
     *
     **%apidoc PUT /rest/v{1-2}/userRoles/{id}
     * Replaces all fields of the specified User Role record stored in the Site.
     * %caption Replace User Role
     * %ingroup User Roles
     * %struct UserRoleModel
     * %param:uuid id User Role id (can be obtained from "id" field via
     *     `GET /rest/v{1-2}/userRoles`).
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserRoleModel} User Role record.
     *
     **%apidoc PATCH /rest/v{1-2}/userRoles/{id}
     * Modifies certain fields of the specified User Role record stored in the Site.
     * %caption Modify User Role
     * %ingroup User Roles
     * %struct[opt] UserRoleModel
     * %param:uuid id User Role id (can be obtained from "id" field via
     *     `GET /rest/v{1-2}/userRoles`).
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserRoleModel} User Role record.
     *
     **%apidoc DELETE /rest/v{1-2}/userRoles/{id}
     * Deletes the specified User Role record from the Site.
     * %caption Delete User Role
     * %ingroup User Roles
     * %param:uuid id User Role id (can be obtained from "id" field via
     *     `GET /rest/v{1-2}/userRoles`).
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     */
    reg("rest/v{1-2}/userRoles/:id?", GlobalPermission::none, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<nx::vms::server::UserGroupsHandler<
            nx::vms::api::UserRoleModel, QueryProcessor>>(queryProcessor, systemContext));

    /**%apidoc GET /rest/v{3-}/userGroups
     * Retrieves all User Group records stored in the Site.
     * %caption Get User Groups
     * %ingroup User Groups
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{UserGroupModelList} List of all User Group records.
     *
     **%apidoc POST /rest/v{3-}/userGroups
     * Creates a record in the Site for the new User Group.
     * %caption Create User Group
     * %ingroup User Groups
     * %struct UserGroupModel
     * %param[unused] id
     * %param[unused] externalId
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserGroupModel} User Group record that was created.
     *
     **%apidoc GET /rest/v{3-}/userGroups/{id}
     * Retrieves the specified User Group record stored in the Site.
     * %caption Get User Group
     * %ingroup User Groups
     * %param:uuid id User Group id (can be obtained from "id" field via
     *     `GET /rest/v{3-}/userGroups`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{UserGroupModel} User Group record.
     *
     **%apidoc PUT /rest/v{3-}/userGroups/{id}
     * Replaces all fields of the specified User Group record stored in the Site.
     * %caption Replace User Group
     * %ingroup User Groups
     * %struct UserGroupModel
     * %param id User Group id (can be obtained from "id" field via
     *     `GET /rest/v{3-}/userGroups`).
     * %param[unused] externalId
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserGroupModel} User Group record.
     *
     **%apidoc PATCH /rest/v{3-}/userGroups/{id}
     * Modifies certain fields of the specified User Group record stored in the Site.
     * %caption Modify User Group
     * %ingroup User Groups
     * %struct[opt] UserGroupModel
     * %param id User Group id (can be obtained from "id" field via
     *     `GET /rest/v{3-}/userGroups`).
     * %param[unused] externalId
     * %param[unused] type
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{UserGroupModel} User Group record.
     *
     **%apidoc DELETE /rest/v{3-}/userGroups/{id}
     * Deletes the specified User Group record from the Site.
     * %caption Delete User Group
     * %ingroup User Groups
     * %param:uuid id User Group id (can be obtained from "id" field via
     *     `GET /rest/v{3-}/userGroups`).
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     */
    reg("rest/v{3-}/userGroups/:id?", GlobalPermission::none, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<nx::vms::server::UserGroupsHandler<
            nx::vms::api::UserGroupModel, QueryProcessor>>(queryProcessor, systemContext));

    /**%apidoc GET /rest/v{1-}/layouts
     * Retrieves all Layout records stored in the Site.
     * %caption Get Layouts
     * %ingroup Layouts
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{LayoutDataList} List of all Layout records.
     *
     **%apidoc POST /rest/v{1-}/layouts
     * Creates a record in the Site about the new Layout.
     * %caption Create Layout
     * %ingroup Layouts
     * %struct LayoutData
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{LayoutData} Layout record that was created.
     *
     **%apidoc GET /rest/v{1-}/layouts/{id}
     * Retrieves the specified Layout record stored in the Site.
     * %caption Get Layout
     * %ingroup Layouts
     * %param:string id Layout unique id or logical id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{LayoutData} Layout record.
     *
     **%apidoc PUT /rest/v{1-}/layouts/{id}
     * Replaces all fields of the specified Layout record stored in the Site.
     * %caption Replace Layout
     * %ingroup Layouts
     * %struct LayoutData
     * %param:string id Layout unique id or logical id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{LayoutData} Layout record.
     *
     **%apidoc PATCH /rest/v{1-}/layouts/{id}
     * Modifies certain fields of the specified Layout record stored in the Site.
     * %caption Modify Layout
     * %ingroup Layouts
     * %struct [opt] LayoutData
     * %param:string id Layout unique id or logical id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{LayoutData} Layout record.
     *
     **%apidoc DELETE /rest/v{1-}/layouts/{id}
     * Deletes the specified Layout record from the Site.
     * %caption Delete Layout
     * %ingroup Layouts
     * %param:string id Layout unique id or logical id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{1-}/layouts/:id?", GlobalPermission::none,
        std::make_unique<nx::vms::server::LayoutsHandler<QueryProcessor>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v{2-}/layouts/{layoutId}/items
     * Retrieves all Layout Items for the specified Layout record stored in the Site.
     * %caption Get Layout Items
     * %ingroup Layouts
     * %param:string layoutId Layout unique id or logical id.
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{LayoutItemModelList} List of all Layout Items.
     *
     **%apidoc GET /rest/v{2-}/layouts/{layoutId}/items/{id}
     * Retrieves the specified Layout Item for the specified Layout record stored in the Site.
     * %caption Get Layout Item
     * %ingroup Layouts
     * %struct LayoutItemFilter
     * %param:string layoutId Layout unique id or logical id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{LayoutItemModel} Layout Item.
     *
     **%apidoc POST /rest/v{2-}/layouts/{layoutId}/items
     * Adds a new Layout Item record to the specified Layout stored in the Site.
     * %caption Add Layout Item to Layout
     * %ingroup Layouts
     * %struct LayoutItemModel
     * %param:string layoutId Layout unique id or logical id.
     * %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{LayoutItemModel} Layout Item.
     *
     **%apidoc PUT /rest/v{2-}/layouts/{layoutId}/items/{id}
     * Replaces all fields of the specified Layout Item for the specified Layout record stored
     * in the Site.
     * %caption Replace Layout Item
     * %ingroup Layouts
     * %struct LayoutItemModel
     * %param:string layoutId Layout unique id or logical id.
     * %param id Layout Item unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{LayoutItemModel} Layout Item.
     *
     **%apidoc PATCH /rest/v{2-}/layouts/{layoutId}/items/{id}
     * Modifies certain fields of the specified Layout Item for the specified Layout record
     * stored in the Site.
     * %caption Modify Layout Item
     * %ingroup Layouts
     * %struct[opt] LayoutItemModel
     * %param:string layoutId Layout unique id or logical id.
     * %param id Layout Item unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{LayoutItemModel} Layout Item.
     *
     **%apidoc DELETE /rest/v{2-}/layouts/{layoutId}/items/{id}
     * Deletes the specified Layout Item for the specified Layout record from the Site.
     * %caption Delete Layout Item
     * %ingroup Layouts
     * %struct LayoutItemFilter
     * %param:string layoutId Layout unique id or logical id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{2-}/layouts/:layoutId/items/:id?", GlobalPermission::none,
        std::make_unique<nx::vms::server::LayoutItemsHandler<QueryProcessor>>(
            queryProcessor, systemContext));

    /**%apidoc GET /rest/v1/layoutTours
     * Retrieves Showreel records stored in the Site.
     * %caption Get Showreel records
     * %ingroup Showreels
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelDataList} List of all Showreel records.
     *
     **%apidoc POST /rest/v1/layoutTours
     * Creates a record in the Site for the new Showreel.
     * %caption Create Showreel record
     * %ingroup Showreels
     * %struct ShowreelData
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record that was created.
     *
     **%apidoc GET /rest/v1/layoutTours/{id}
     * Retrieves the specified Showreel record stored in the Site.
     * %caption Get Showreel record
     * %ingroup Showreels
     * %param:uuid id Showreel unique id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record.
     *
     **%apidoc PUT /rest/v1/layoutTours/{id}
     * Replaces all fields of the specified Showreel record stored in the Site.
     * %caption Replace Showreel record
     * %ingroup Showreels
     * %struct ShowreelData
     * %param:uuid id Showreel unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record.
     *
     **%apidoc PATCH /rest/v1/layoutTours/{id}
     * Modifies certain fields of the specified Showreel record stored in the Site.
     * %caption Modify Showreel record
     * %ingroup Showreels
     * %struct [opt] ShowreelData
     * %param:uuid id Showreel unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record.
     *
     **%apidoc DELETE /rest/v1/layoutTours/{id}
     * Deletes the specified Showreel record from the Site.
     * %caption Delete Showreel record
     * %ingroup Showreels
     * %param:uuid id Showreel unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v1/layoutTours/:id?", GlobalPermission::none,
        std::make_unique<nx::vms::server::ShowreelsHandler<QueryProcessor>>(queryProcessor));

    /**%apidoc GET /rest/v{2-}/showreels
     * Retrieves all Showreel records stored in the Site.
     * %caption Get Showreels
     * %ingroup Showreels
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelDataList} List of all Showreel records.
     *
     **%apidoc POST /rest/v{2-}/showreels
     * Creates a record in the Site for the new Showreel.
     * %caption Create Showreel
     * %ingroup Showreels
     * %struct ShowreelData
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record that was created.
     *
     **%apidoc GET /rest/v{2-}/showreels/{id}
     * Retrieves the specified Showreel record stored in the Site.
     * %caption Get Showreel
     * %ingroup Showreels
     * %param:uuid id Showreel unique id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record.
     *
     **%apidoc PUT /rest/v{2-}/showreels/{id}
     * Replaces all fields of the specified Showreel record stored in the Site.
     * %caption Replace Showreel
     * %ingroup Showreels
     * %struct ShowreelData
     * %param:uuid id Showreel unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record.
     *
     **%apidoc PATCH /rest/v{2-}/showreels/{id}
     * Modifies certain fields of the specified Showreel record stored in the Site.
     * %caption Modify Showreel
     * %ingroup Showreels
     * %struct[opt] ShowreelData
     * %param:uuid id Showreel unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{ShowreelData} Showreel record.
     *
     **%apidoc DELETE /rest/v{2-}/showreels/{id}
     * Deletes the specified Showreel record from the Site.
     * %caption Delete Showreel
     * %ingroup Showreels
     * %param:uuid id Showreel unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{2-}/showreels/:id?", GlobalPermission::none,
        std::make_unique<nx::vms::server::ShowreelsHandler<QueryProcessor>>(queryProcessor));

    /**%apidoc GET /rest/v{3-}/lookupLists
     * Retrieves Lookup List records stored in the Site.
     * %caption Get Lookup List
     * %ingroup Lookup Lists
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{LookupListDataList} List of all Lookup List records.
     *
     **%apidoc POST /rest/v{3-}/lookupLists
     * Creates a record in the Site for the new Lookup List.
     * %caption Create Lookup List
     * %ingroup Lookup Lists
     * %struct LookupListData
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{LookupListData} Lookup List record that was created.
     *
     **%apidoc GET /rest/v{3-}/lookupLists/{id}
     * Retrieves the specified Lookup List record stored in the Site.
     * %caption Get Lookup List
     * %ingroup Lookup Lists
     * %param:uuid id Lookup List unique id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{LookupListData} Lookup List record.
     *
     **%apidoc PUT /rest/v{3-}/lookupLists/{id}
     * Replaces all fields of the specified Lookup List record stored in the Site.
     * %caption Replace Lookup List
     * %ingroup Lookup Lists
     * %struct LookupListData
     * %param:uuid id Lookup List unique id.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{LookupListData} Lookup List record.
     *
     **%apidoc PATCH /rest/v{3-}/lookupLists/{id}
     * Modifies certain fields of the specified Lookup List record stored in the Site.
     * %caption Modify Lookup List
     * %ingroup Lookup Lists
     * %struct [opt] LookupListData
     * %param:uuid id Lookup List unique id.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{LookupListData} Lookup List record.
     *
     **%apidoc DELETE /rest/v{3-}/lookupLists/{id}
     * Deletes the specified Lookup List record from the Site.
     * %caption Delete Lookup List
     * %ingroup Lookup Lists
     * %param:uuid id Lookup List unique id.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{3-}/lookupLists/:id?",
        GlobalPermission::powerUser,
        std::make_unique<nx::vms::server::LookupListsHandler<QueryProcessor>>(
            queryProcessor));

    /**%apidoc GET /rest/v{1-}/videoWalls
     * Retrieves all Video Wall records stored in the Site.
     * %caption Get Video Walls
     * %ingroup Video Walls
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{VideowallDataList} List of all Video Wall records.
     *
     **%apidoc POST /rest/v{1-}/videoWalls
     * Creates a record in the Site for the new Video Wall.
     * %caption Create Video Wall
     * %ingroup Video Walls
     * %struct VideowallData
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{VideowallData} Video Wall record that was created.
     *
     **%apidoc GET /rest/v{1-}/videoWalls/{id}
     * Retrieves the specified Video Wall record stored in the Site.
     * %caption Get Video Wall
     * %ingroup Video Walls
     * %param:uuid id Video Wall unique id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{VideowallData} Video Wall record.
     *
     **%apidoc PUT /rest/v{1-}/videoWalls/{id}
     * Replaces all fields of the specified Video Wall record stored in the Site.
     * %caption Replace Video Wall
     * %ingroup Video Walls
     * %struct VideowallData
     * %param:uuid id Video Wall unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{VideowallData} Video Wall record.
     *
     **%apidoc PATCH /rest/v{1-}/videoWalls/{id}
     * Modifies certain fields of the specified Video Wall record stored in the Site.
     * %caption Modify Video Wall
     * %ingroup Video Walls
     * %struct [opt] VideowallData
     * %param id Video Wall unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{VideowallData} Video Wall record.
     *
     **%apidoc DELETE /rest/v{1-}/videoWalls/{id}
     * Deletes the specified Video Wall record from the Site.
     * %caption Delete Video Wall
     * %ingroup Video Walls
     * %param:uuid id Video Wall unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{1-}/videoWalls/:id?", GlobalPermission::none,
        std::make_unique<nx::vms::server::VideoWallsHandler<QueryProcessor>>(
            queryProcessor));

    /**%apidoc GET /rest/v{1-2}/webPages
     * Retrieves all Web Page records stored in the Site.
     * %caption Get Web Pages
     * %ingroup Web Pages
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<WebPageModelV1>} List of all Web Page records.
     *
     **%apidoc POST /rest/v{1-2}/webPages
     * Creates a record in the Site for the new Web Page.
     * %caption Create Web Page
     * %ingroup Web Pages
     * %struct WebPageModelV1
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV1} Web Page record that was created.
     *
     **%apidoc GET /rest/v{1-2}/webPages/{id}
     * Retrieves the specified Web Page record stored in the Site.
     * %caption Get Web Page
     * %ingroup Web Pages
     * %param:uuid id Web Page unique id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV1} Web Page record.
     *
     **%apidoc PUT /rest/v{1-2}/webPages/{id}
     * Replaces all fields of the specified Web Page record stored in the Site.
     * %caption Replace Web Page
     * %ingroup Web Pages
     * %struct WebPageModelV1
     * %param:uuid id Web Page unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV1} Web Page record.
     *
     **%apidoc PATCH /rest/v{1-2}/webPages/{id}
     * Modifies certain fields of the specified Web Page record stored in the Site.
     * %caption Modify Web Page
     * %ingroup Web Pages
     * %struct [opt] WebPageModelV1
     * %param:uuid id Web Page unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV1} Web Page record.
     *
     **%apidoc DELETE /rest/v{1-2}/webPages/{id}
     * Deletes the specified Web Page record from the Site.
     * %caption Delete Web Page
     * %ingroup Web Pages
     * %param:uuid id Web Page unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{1-2}/webPages/:id?",
        GlobalPermission::none,
        std::make_unique<
            nx::vms::server::WebPagesHandler<QueryProcessor, nx::vms::api::WebPageModelV1>>(
            queryProcessor));

    /**%apidoc GET /rest/v{3-}/webPages
     * Retrieves all Web Page records stored in the Site.
     * %caption Get Web Pages
     * %ingroup Web Pages
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<WebPageModelV3>} List of all Web Page records.
     *
     **%apidoc POST /rest/v{3-}/webPages
     * Creates a record in the Site for the new Web Page.
     * %caption Create Web Page
     * %ingroup Web Pages
     * %struct WebPageModelV3
     * %param [unused] id
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV3} Web Page record that was created.
     *
     **%apidoc GET /rest/v{3-}/webPages/{id}
     * Retrieves the specified Web Page record stored in the Site.
     * %caption Get Web Page
     * %ingroup Web Pages
     * %param:uuid id Web Page unique id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV3} Web Page record.
     *
     **%apidoc PUT /rest/v{3-}/webPages/{id}
     * Replaces all fields of the specified Web Page record stored in the Site.
     * %caption Replace Web Page
     * %ingroup Web Pages
     * %struct WebPageModelV3
     * %param:uuid id Web Page unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV3} Web Page record.
     *
     **%apidoc PATCH /rest/v{3-}/webPages/{id}
     * Modifies certain fields of the specified Web Page record stored in the Site.
     * %caption Modify Web Page
     * %ingroup Web Pages
     * %struct [opt] WebPageModelV3
     * %param:uuid id Web Page unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{WebPageModelV3} Web Page record.
     *
     **%apidoc DELETE /rest/v{3-}/webPages/{id}
     * Deletes the specified Web Page record from the Site.
     * %caption Delete Web Page
     * %ingroup Web Pages
     * %param:uuid id Web Page unique id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{3-}/webPages/:id?",
        GlobalPermission::none,
        std::make_unique<
            nx::vms::server::WebPagesHandler<QueryProcessor, nx::vms::api::WebPageModelV3>>(
            queryProcessor));

    /**%apidoc GET /rest/v1/system/database
     * Retrieves the binary dump of the Site database (shared among all Servers).
     * <br/>
     * ATTENTION: The database data depends on the proprietary database structure and is not
     * intended for manual modification. This data is only intended for restoring via
     * `POST /rest/v1/system/database`.
     * <br/>
     * ATTENTION: This API function may produce output of significant size, so some of UI
     * tools like API testing tools may freeze on processing. Consider executing directly in the
     * browser as a workaround.
     *
     * %caption Dump Site database
     * %ingroup Site
     * %param[ref] _language,_strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{DatabaseDumpData}
     *
     **%apidoc POST /rest/v1/system/database
     * Loads the Site database (shared among all Servers) from the binary dump provided. The
     * Server will be restarted after loading.
     * %caption Load Site database
     * %ingroup Site
     * %struct DatabaseDumpData
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %// NOTE: Administrator or Power User is checked by the handler based on adminApiForPowerUsers
     * setting.
     */
    reg("rest/v1/system/database", GlobalPermission::requireFreshSession,
        std::make_unique<nx::vms::server::SystemDatabaseHandlerV1<QueryProcessor>>(
            queryProcessor));

    /**%apidoc GET /rest/v{2-3}/system/database
     * Retrieves the binary dump of the Site database (shared among all Servers).
     * <br/>
     * ATTENTION: The database data depends on proprietary database structure and is not
     * intended for manual modification. This data is only intended for restoring via
     * `POST /rest/v{2-3}/system/database`.
     *
     * %caption Dump Site database
     * %ingroup Site
     * %param[ref] _language,_strict,_ticket
     * %permissions Administrator with a fresh session.
     *
     **%apidoc:binary POST /rest/v{2-3}/system/database
     * Loads the Site database (shared among all Servers) from the binary dump provided. The
     * Server will be restarted after loading.
     * %caption Load Site database
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %// NOTE: Administrator or Power User is checked by the handler based on the
     *     adminApiForPowerUsers setting.
     */
    reg("rest/v{2-3}/system/database", GlobalPermission::requireFreshSession,
        std::make_unique<nx::vms::server::SystemDatabaseHandlerV2<QueryProcessor>>(
            queryProcessor));

    /**%apidoc GET /rest/v{4-}/site/database
     * Retrieves the binary dump of the Site database (shared among all Servers).
     * <br/>
     * ATTENTION: The database data depends on proprietary database structure and is not
     * intended for manual modification. This data is only intended for restoring via
     * `POST /rest/v{4-}/site/database`.
     *
     * %caption Dump Site database
     * %ingroup Site
     * %param[ref] _language,_strict,_ticket
     * %permissions Administrator with a fresh session.
     *
     **%apidoc:binary POST /rest/v{4-}/site/database
     * Loads the Site database (shared among all Servers) from the binary dump provided. The
     * Server will be restarted after loading.
     * %caption Load Site database
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %// NOTE: Administrator or Power User is checked by the handler based on the
     *     adminApiForPowerUsers setting.
     */
    reg("rest/v{4-}/site/database", GlobalPermission::requireFreshSession,
        std::make_unique<nx::vms::server::SystemDatabaseHandlerV2<QueryProcessor>>(
            queryProcessor));

    /**%apidoc[proprietary] GET /rest/v{2-3}/system/databaseData
     * Retrieves the structured data of the Site database (shared among all Servers).
     * <br/>
     * ATTENTION: The database data depends on the proprietary database structure which may
     * change in any VMS Server version without further notice. Use at your own risk.
     * <br/>
     * ATTENTION: This API function may produce output of significant size, so some of UI
     * tools like API testing tools may freeze on processing. Consider executing directly in the
     * browser as a workaround.
     *
     * %caption Inspect Site database
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{FullInfoData}
     */
    reg("rest/v{2-3}/system/databaseData", GlobalPermission::none,
        std::make_unique<nx::vms::server::SystemDatabaseDataHandler<QueryProcessor>>(
            queryProcessor));

    /**%apidoc[proprietary] GET /rest/v{4-}/site/databaseData
     * Retrieves the structured data of the Site database (shared among all Servers).
     * <br/>
     * ATTENTION: The database data depends on the proprietary database structure which may
     * change in any VMS Server version without further notice. Use at your own risk.
     * <br/>
     * ATTENTION: This API function may produce output of significant size, so some of UI
     * tools like API testing tools may freeze on processing. Consider executing directly in the
     * browser as a workaround.
     *
     * %caption Inspect Site database
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{FullInfoData}
     */
    reg("rest/v{4-}/site/databaseData", GlobalPermission::none,
        std::make_unique<nx::vms::server::SystemDatabaseDataHandler<QueryProcessor>>(
            queryProcessor));

    /**%apidoc[proprietary] GET /rest/v{2-3}/system/transactionLog
     * Retrieves Site database transactions. The response structure may change in the future VMS
     * versions.
     * <br/>
     * ATTENTION: This API function may produce output of significant size, so some of UI tools
     * like API testing tools may freeze on processing. Consider executing directly in the browser
     * as a workaround.
     *
     * %caption Read DB Transactions
     * %ingroup Site
     * %param[opt]:option cloudOnly Show only the transactions that are synced to the Cloud.
     * %param[opt]:option removeOnly Show only the transactions that remove objects.
     * %param[opt]:option withData Fill response `data` field with transaction data deserialized.
     * %param[ref] _filter,_format,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User or Administrator with a fresh session if `data` is required.
     * %return:{std::vector<TransactionModel>} Site database transactions.
     * %// NOTE: Permissions are checked by the handler.
     */
    reg("rest/v{2-3}/system/transactionLog", GlobalPermission::none,
        std::make_unique<nx::vms::server::TransactionLogHandler>(connection));

    /**%apidoc[proprietary] GET /rest/v{4-}/site/transactionLog
     * Retrieves Site database transactions. The response structure may change in the future VMS
     * versions.
     * <br/>
     * ATTENTION: This API function may produce output of significant size, so some of UI tools
     * like API testing tools may freeze on processing. Consider executing directly in the browser
     * as a workaround.
     *
     * %caption Read DB Transactions
     * %ingroup Site
     * %param[opt]:option cloudOnly Show only the transactions that are synced to the Cloud.
     * %param[opt]:option removeOnly Show only the transactions that remove objects.
     * %param[opt]:option withData Fill response `data` field with transaction data deserialized.
     * %param[ref] _filter,_format,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User or Administrator with a fresh session if `data` is required.
     * %return:{std::vector<TransactionModel>} Site database transactions.
     * %// NOTE: Permissions are checked by the handler.
     */
    reg("rest/v{4-}/site/transactionLog", GlobalPermission::none,
        std::make_unique<nx::vms::server::TransactionLogHandler>(connection));
}
