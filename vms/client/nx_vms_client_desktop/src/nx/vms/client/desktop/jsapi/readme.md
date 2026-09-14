@mainpage In-client JavaScript API specification

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

Specification for the JavaScript API which can be used inside web pages opened in the Desktop
Client.

# Basic concepts

## Global objects

The API consist of some global objects injected to the client JavaScript code. They are:

| Global object name | Description                                                   |
| ------------------ | ------------------------------------------------------------- |
| @ref vms           | Contains global constants and methods.                        |
| @ref vms-log       | Allows to log messages using the standard VMS logging system. |
| @ref vms-resources | Object for the Resource management purposes.                  |
| @ref vms-tabs      | Contains methods to work with multiple tabs.                  |
| @ref vms-tab       | Objects for the currently opened tab management.              |
| @ref vms-self      | Contains methods to control the current web-page widget.      |
| @ref vms-auth      | Allows to get the authentication data.                        |

## Entry point

The JavaScript API calls `window.vmsApiInit()` to signalize that the API is ready to be used.
Before this call, global objects of the API are not available. This function should be initialized
by the user by assigning a callback. There is also a `window.isVmsApiEnabled` flag, which can be
used to check whether the API is enabled and will be initialized.

    if (window.isVmsApiEnabled)
    {
        window.vmsApiInit =
            async function()
            {
                vms.log.info("Here we have all the API objects initialized")
            }
    }

## Cloud Layouts {#cloud-layouts}

Resources from other Sites can be placed on a Cloud Layout only. An attempt to add such a Resource
to a regular Layout with @ref vms-tab `addItem()` fails.

A Layout cannot be converted into a Cloud one in place. Instead, it is replaced with its cloud copy
and reopened, so all Layout items, including the web page which requests the operation, are
recreated. Any state of the web page which is not stored by the page itself is lost.

Thus, an integration which is going to use Resources from other Sites should check the Layout type
when it is loaded, and ask the user whether the Layout may be reopened as a Cloud one:

    if (!vms.tab.cloudLayout && confirm("The Layout is to be reopened as a Cloud one. Continue?"))
        await vms.tab.reopenAsCloudLayout()

The reopened Layout is not saved automatically, use @ref vms-tab `saveLayout()` if it is required.

Only cloud users can use Cloud Layouts, so for a local user the check should be omitted along with
any functionality which relies on Resources from other Sites.

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
@defgroup vms-tabs vms.tabs
@defgroup vms-tab vms.tab
@defgroup vms-self vms.self
@defgroup vms-auth vms.auth
