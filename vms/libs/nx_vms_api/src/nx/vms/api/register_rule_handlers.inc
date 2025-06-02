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
     * %return:{EventLogRecordV3List} Server event log.
     */
    reg("rest/v3/servers/:serverId/events", GlobalPermission::viewLogs,
        std::make_unique<EventLogHandlerOldFilter>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/events/manifest/events
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %jsonrpc[result] all
     * %caption Get Event manifest
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{QMap<QString, ItemDescriptor>}
     */
    reg("rest/v{4-}/events/manifest/events", GlobalPermission::powerUser,
        std::make_unique<EventManifestHandler>(engine));

    /**%apidoc GET /rest/v{4-}/events/manifest/actions
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %jsonrpc[result] all
     * %caption Get Action manifest
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{QMap<QString, ItemDescriptor>}
     */
    reg("rest/v{4-}/events/manifest/actions", GlobalPermission::powerUser,
        std::make_unique<ActionManifestHandler>(engine));

    /**%apidoc GET /rest/v{4-}/events/log
     * Read Event Log from all Servers.
     * %jsonrpc subscribe Same result and `update` notifications over time.
     * %caption Get Event Log
     * %ingroup Events
     * %struct EventLogFilter
     * %param[unused] serverId
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions Event log and audit trail view.
     * %return:{EventLogRecordList} Server event log.
     *
     **%apidoc GET /rest/v{4-}/events/log/{serverId}
     * Read Event Log from single Server.
     * %jsonrpc subscribe Same result and `update` notifications over time.
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
     * %jsonrpc subscribe Same result and `update` notifications over time.
     * %caption Notifications to acknowledge
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_with,_strict,_ticket,_local
     * %permissions Any User.
     * %return:{EventLogRecordList} Notifications to acknowledge
     *
     **%apidoc GET /rest/v{4-}/events/acknowledges/{id}
     * Retrieves notification with the given event ID.
     * %jsonrpc subscribe Same result and `update` notifications over time.
     * %caption Notification to acknowledge
     * %ingroup Events
     * %param:uuid id Event ID.
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

    /**%apidoc POST /rest/v{4-}/events/generic
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
     * %struct GenericEventData
     *     %param state
     *         %value[unused] none
     * %param[ref] _strict,_ticket
     * %permissions Generate events global permission.
     */
    reg("rest/v{4-}/events/generic", GlobalPermission::generateEvents,
        std::make_unique<GenericEventHandler>(serverModule->systemContext()));

    /**%apidoc GET /rest/v{4-}/events/triggers
     * Retrieves software triggers available to the given user.
     * %jsonrpc subscribe
     * %caption Software triggers
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_with,_strict,_ticket,_local
     * %permissions Any User.
     * %return:{SoftTriggerInfoList} Software triggers
     *
     **%apidoc GET /rest/v{4-}/events/triggers/{id}
     * Retrieves software trigger available to the given user.
     * %caption Software trigger
     * %ingroup Events
     * %param:uuid id Trigger ID.
     * %param[ref] _format,_stripDefault,_language,_pretty,_with,_strict,_ticket,_local
     * %permissions Any User.
     * %return:{SoftTriggerInfo} Software trigger
     *
     **%apidoc POST /rest/v{4-}/events/triggers
     * %caption Create soft trigger event
     * %ingroup Events
     * %struct SoftTriggerData
     *     %param state
     *         %value[unused] none
     * %param[ref] _strict,_ticket,_local
     * %permissions Software trigger permission for the given device.
     */
    reg("rest/v{4-}/events/triggers/:id?", GlobalPermission::none,
        std::make_unique<SoftTriggerHandler>(serverModule->systemContext(), serverConnector));

    /**%apidoc POST /rest/v{4-}/events/rules
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * <br/>
     * A rule can only handle events that are created after the rule itself is created.
     * For example, if an Generic event was created and entered the 'started' state before a rule
     * was created, that later-created rule cannot handle the event when it subsequently moves to
     * the 'stopped' state.
     * %caption Create Rule
     * %ingroup Events
     * %struct RuleV4
     *     %param[unused] id
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc PUT /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Replace Rule
     * %ingroup Events
     * %struct RuleV4
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc PATCH /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Modify Rule
     * %ingroup Events
     * %struct[opt] RuleV4
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc GET /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %jsonrpc subscribe
     * %caption Get Rule
     * %ingroup Events
     * %param:string id
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{RuleV4}
     *
     **%apidoc GET /rest/v{4-}/events/rules
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %jsonrpc subscribe
     * %caption Get all Rules
     * %ingroup Events
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{RuleV4List}
     *
     **%apidoc DELETE /rest/v{4-}/events/rules/{id}
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Remove Rule
     * %ingroup Events
     * %param:string id
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/events/rules/:id?", GlobalPermission::powerUser,
        std::make_unique<SimpleRuleHandler<QueryProcessor>>(
            serverModule->directConnection()->queryProcessor(), serverModule->systemContext()));

    /**%apidoc POST /rest/v{4-}/events/rules/%2A/reset
     * <b>ATTENTION:</b> Works with new Event Rule Engine only.
     * %caption Reset all rules to default
     * %ingroup Events
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/events/rules/*/reset", GlobalPermission::powerUser,
        std::make_unique<ResetRulesHandler>(serverModule->systemContext()));
}
