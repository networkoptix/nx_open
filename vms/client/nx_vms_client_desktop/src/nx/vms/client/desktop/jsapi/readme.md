@mainpage In-client JavaScript API specification

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

Specification for the JavaScript API which can be used inside web pages opened in the Desktop
Client.

# Basic concepts

## Global objects

The API consist of some global objects injected to the client JavaScript code. They are:

| Global object name | Description                                                   |
| ------------------ | ------------------------------------------------------------- |
| @ref vms           | Contains global constants.                                    |
| @ref vms-log       | Allows to log messages using the standard VMS logging system. |
| @ref vms-resources | Object for the Resource management purposes.                  |
| @ref vms-tab       | Objects for the currently opened tab management.              |
| @ref vms-self      | Contains methods to control the current web-page widget.      |
| @ref vms-auth      | Allows to get the authentication data.                        |

## Entry point

The JavaScript API calls `window.vmsApiInit()` to signalize that the API is ready to be used.
Before this call, global objects of the API are not available. This function should be initialized
by the user by assigning a callback.

    window.vmsApiInit =
        async function()
        {
            vms.log.info("Here we have all the API objects initialized")
        }

## Signals

The API supports **signals**. These objects are used like event handlers and have the
`connect(callback)` function. Signals may be used to handle some events.

    vms.resources.added.connect(
        function(resource)
        {
            vms.log.info(`Added a new Resource: ${resource.name}`)
        })

@defgroup vms vms
@defgroup vms-log vms.log
@defgroup vms-resources vms.resources
@defgroup vms-tab vms.tab
@defgroup vms-self vms.self
@defgroup vms-auth vms.auth
