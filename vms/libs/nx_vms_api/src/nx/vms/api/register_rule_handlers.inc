// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

void registerRuleHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    nx::vms::network::AbstractServerConnector* serverConnector)
{
    using QueryProcessor = ec2::ServerQueryProcessorAccess;
    const auto engine = serverModule->vmsRulesEngine();
    const auto reg = processorPool->createRegisterHandlerFunctor();

    // TODO: #amalov Fix the handle apidoc return type when removing old engine handler.
    std::unique_ptr<nx::network::rest::Handler> v3LogHandler;
    if (std::string_view{nx::vms::rules::ini().rulesEngine} != "new")
        v3LogHandler = std::make_unique<EventLogHandlerOldEngine>(serverModule, serverConnector);
    else
        v3LogHandler = std::make_unique<EventLogHandlerOldFilter>(serverModule, serverConnector);

    /**%apidoc GET /rest/v3/servers/{serverId}/events
     * %caption Read Server Event Log
     * %ingroup Servers
     * %struct EventLogFilterDeprecated
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers`, or be `this` to refer to the current Server, or be "*" to
     *     involve all Servers.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Event log and audit trail view.
     * %return Server event log.
     */
    reg("rest/v3/servers/:serverId/events", GlobalPermission::viewLogs,
        std::move(v3LogHandler));

    /**%apidoc[proprietary] GET /rest/v{4-}/events/manifest/events
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Get Event manifest
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{QMap<QString, ItemDescriptor>}
     */
    reg("rest/v{4-}/events/manifest/events", GlobalPermission::powerUser,
        std::make_unique<EventManifestHandler>(engine));

    /**%apidoc[proprietary] GET /rest/v{4-}/events/manifest/actions
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Get Action manifest
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{QMap<QString, ItemDescriptor>}
     */
    reg("rest/v{4-}/events/manifest/actions", GlobalPermission::powerUser,
        std::make_unique<ActionManifestHandler>(engine));

    /**%apidoc GET /rest/v{4-}/events/log
     * Read Event Log from all servers
     * %caption Get Event Log
     * %ingroup Events
     * %struct EventLogFilter
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions Event log and audit trail view.
     * %return:{EventLogRecordList} Server event log.
     *
     **%apidoc GET /rest/v{4-}/events/log/{serverId}
     * Read Event Log from single server
     * %caption Get Event Log
     * %ingroup Events
     * %struct EventLogFilter
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers`, or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions Event log and audit trail view.
     * %return:{EventLogRecordList} Server event log.
     */
    reg("rest/v{4-}/events/log/:serverId?", GlobalPermission::viewLogs,
        std::make_unique<EventLogHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/events/acknowledges
     * Retrieves notifications to acknowledge for the given user.
     * %caption Notifications to acknowledge
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_with,_strict,_ticket,_local
     * %permissions Any User.
     * %return:{EventLogRecordList} Notifications to acknowledge
     *
     **%apidoc GET /rest/v{4-}/events/acknowledges/{id}
     * Retrieves notification with the given action ID.
     * %caption Notification to acknowledge
     * %ingroup Events
     * %param:uuid id Action ID.
     * %param[ref] _format,_stripDefault,_language,_pretty,_with,_strict,_ticket,_local
     * %permissions Any User.
     * %return:{EventLogRecord} Notification to acknowledge
     *
     **%apidoc POST /rest/v{4-}/events/acknowledges
     * Marks event as acknowledged and creates corresponding bookmark.
     * %caption Acknowledge Event notification
     * %ingroup Events
     * %struct AcknowledgeBookmark
     * %param[ref] _strict,_ticket,_local
     * %permissions Target user for the event, permission to add bookmark for the device.
     * %return:{BookmarkV3} Created bookmark data
     */
    reg("rest/v{4-}/events/acknowledges/:id?", GlobalPermission::none,
        std::make_unique<AcknowledgeHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/events/create
     * This method may trigger an event of the "Generic Event" type in the Site from a 3rd party.
     * Such Event will be handled and logged according to current VMS Rules.
     * Parameters of the generated Event, such as "source", "caption" and "description", are
     * intended to be analyzed by these Rules.
     * <br/> Parameters should be passed as a JSON object in POST message body with content type
     * "application/json". Example:
     * <pre><code>
     * {
     *     "state": "instant",
     *     "timestamp": "2024-06-16T16:02:41Z",
     *     "caption": "CreditCardUsed",
     *     "deviceIds": [
     *         "3A4AD4EA-9269-4B1F-A7AA-2CEC537D0248",
     *         "3A4AD4EA-9269-4B1F-A7AA-2CEC537D0240"
     *     ]
     * }
     * </code></pre>
     * This example triggers a generic Event informing the Site that a
     * credit card has been used on June 16, 2024 at 16:03:41 UTC in a POS
     * terminal being watched by the two specified Devices.
     * %caption Create generic event
     * %ingroup Events
     * %param[opt]:string type Type of the Event to be created. Omit the parameter for
     *     Generic Event. See /rest/v{4-}/events/manifest/events for event manifests with possible
     *     event types.
     * %param[opt]:any timestamp Event date and time, as a number containing time in
     *     microseconds since epoch, or an ISO timestamp string formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     *     the format is auto-detected. If "timestamp" is absent or "now", the current Server
     *     date and time are used.
     * %param[opt]:string source Name of the Device which has triggered the Event. It can be used
     *     in a filter in VMS Rules to assign different actions to different Devices. Also, the
     *     user could see this name in the notification panel. Example: "POS terminal 5".
     * %param[opt]:string caption Short Event description. It can be used in a filter in VMS
     *     Rules to assign actions depending on this text.
     * %param[opt]:string description Long Event description. It can be used as a filter in VMS
     *     Rules to assign actions depending on this text.
     * %param[opt]:array deviceIds Specifies the list of Devices which are linked to the Event (e.g.
     *     the Event will appear on their timelines), in the form of a list of Device ids (can
     *     be obtained from "id" field via /rest/v{1-}/devices).
     * %param[opt]:enum state Generic Events can be used either with "prolonged" actions like
     *     "do recording", or instant actions like "send email". This parameter should be specified
     *     in case "prolonged" actions are going to be used with Generic Events. The Event is
     *     considered instant in case of parameter absence.
     *     %value started Generic Event is considered a "prolonged" Event. It transits to the
     *         "active" state. "Prolonged" actions will continue executing until the Generic Event
     *         transits to the "inactive" state.
     *     %value stopped A "prolonged" action associated with this Generic Event in VMS Rules
     *         will stop. Transition to the <code>stopped</code> state requires the state to be
     *         <code>started</code>.
     * %param[opt]:string cameraId Used for soft trigger.
     * %param[opt]:string triggerId Used for soft trigger.
     * %param[opt]:string triggerName Used for soft trigger.
     * %param[opt]:string triggerIcon Used for soft trigger.
     * %param[opt]:string serverId Used for testing.
     * %param[opt]:string reason Used for testing.
     * %param[ref] _strict,_ticket
     * %permissions Event dependent permissions. Generate events in most cases.
     */
    reg("rest/v{4-}/events/create", GlobalPermission::none,
        std::make_unique<SendEventHandler>(serverModule->systemContext()));

    /**%apidoc[proprietary] POST /rest/v{4-}/events/rules
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Create Rule
     * %ingroup Events
     * %struct RuleV4
     *     %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc[proprietary] PUT /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Replace Rule
     * %ingroup Events
     * %struct RuleV4
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc[proprietary] PATCH /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Modify Rule
     * %ingroup Events
     * %struct[opt] RuleV4
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc[proprietary] GET /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Get Rule
     * %ingroup Events
     * %param:string id
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{RuleV4}
     *
     **%apidoc[proprietary] GET /rest/v{4-}/events/rules
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Get all Rules
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{RuleV4List}
     *
     **%apidoc[proprietary] DELETE /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Remove Rule
     * %ingroup Events
     * %param:string id
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/events/rules/:id?", GlobalPermission::none, GlobalPermission::powerUser,
        std::make_unique<SimpleRuleHandler<QueryProcessor>>(
            serverModule->directConnection()->queryProcessor(), serverModule->systemContext()));

    /**%apidoc[proprietary] POST /rest/v{4-}/events/rules/%2A/reset
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Reset all rules to default
     * %ingroup Events
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/events/rules/*/reset", GlobalPermission::powerUser,
        std::make_unique<ResetRulesHandler>(serverModule->systemContext()));
}
