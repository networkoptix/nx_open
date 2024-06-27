# Analytics Plugin Manifests

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This document describes the schemas of JSON Manifests that are returned by various Analytics
entities - nx::sdk::analytics::IPlugin, IEngine, and IDeviceAgent.

Each Manifest is a JSON object containing fields which can be of the following semantic types:
- String.
- Integer (Number).
- Float (Number) - Floating-point number.
- Boolean.
- Array<Type> - JSON array in `[]`, containing comma-separated items of the specified Type.
- Object - JSON Object in `{}`, containing the described fields.
- Id (String) - String representing a human-readable identifier, described in the respective
    section of this document.
- Flag set (String) - String containing the named flags listed via `|`.
- Enumeration (String) - String containing one of the fixed values.
- SettingsModel (Object) - JSON object of its own schema, described in
    [src/nx/sdk/settings_model.md](@ref md_src_nx_sdk_settings_model).
- TypeLibrary (Object) - JSON object of its own schema, described in
    [src/nx/sdk/analytics/taxonomy.md](@ref md_src_nx_sdk_analytics_taxonomy).

The following rules apply to the Manifest as a JSON document:
- Missing object fields are treated as having a default value. Default values are described in this
    document.
- Unknown object fields are ignored. Thus, if a field name has a typo, it will be ignored and
    the field will be treated as missing, thus, having a default value. It also gives the
    possibility to comment-out a field by renaming it, e.g. prepending with an underscore.

Examples of Manifests can be found in the Plugin samples included with the SDK: a minimalistic
example can be found in Sample Analytics Plugin, and the most comprehensive example covering all
possible features can be found in Stub Analytics Plugin.

---------------------------------------------------------------------------------------------------
## Identifiers

Various entities declared in the Manifests make use of identifiers, including the Plugin itself and
the types of Events and Objects that the Plugin can produce.

Such identifiers, represented in JSON as strings, are intended to be human-readable, English-based,
and formed according to the rules similar to that of Java package names - hierarchical domain-based
chain of dot-separated camelCase components, starting with the organization name (the `java.` part
is obviously not needed, and the organization type part like `com.` or `org.` is not recommended).

The identifiers are intended to be context-local, thus, must be unique only among all identifiers
used to denote entities of the same kind. It means that, for example, there is no need to include
the word `event` in an identifier of certain Event type.

For example, for a company called "My Company", an identifier for an Analytics Event type of a Line
Crossing event may look like `"myCompany.lineCrossing"`.

Identifiers starting with `nx.` must not be used for the entities introduced in the Plugins
developed by parties other than Nx.

There are special, reserved, parts of the identifier name. Identifiers starting with `nx.sys.` are
used for the VMS internal purposes and as a temporary solution to access certain experimental
features. Identifiers with `.sys.` in the middle are recognized by VMS to have some special
behavior.

---------------------------------------------------------------------------------------------------
## Plugin Manifest

Plugin Manifest is a JSON Object containing the following fields:

- `"id"`: Id (String)

    A human-readable identifier of the plugin. According to the basic identifier rules described in
    the corresponding section of this document, it should not include the words `plugin` or the
    like. E.g. for a face recognition plugin developed by "My Company", it may look like
    `"myCompany.faceRecognition"`.

    Mandatory.

- `"name"`: String

    Full name of a plugin as a product, in English, capitalized, using spaces. E.g. for a face
    recognition plugin, it may look like `"Face recognition plugin"`. This name is shown to the user
    in the VMS configuration GUI.

    Mandatory.

- `"description"`: String

    A short text in English describing the purpose of the plugin, typically a single sentense.

    Mandatory.

- `"version"`: String

    An arbitrary string shown to the user, which may be helpful to identify the particular version
    of the plugin. A scheme like `"1.0.2"` can be recommended, but may vary according to the plugin
    vendor needs.

    Optional.

- `"vendor"`: String

    The full name of the organization which is the author of the plugin. May look like e.g.
    `"My Company, Inc."`.

    Mandatory.

- `"engineSettingsModel"`: SettingsModel (Object)

    Describes the names, types, and default values of the settings that an Engine of this plugin
    may have.

    Optional. If omitted or empty, means there are no settings.

---------------------------------------------------------------------------------------------------
## Engine Manifest

Engine Manifest is a JSON Object containing the following fields:

- `"capabilities"`: Flag set (String)

    A combination of zero or more of the following flags, separated with `|`:

    - Flags defining the format of video frames that the Server supplies to the Plugin. No more
        than one of the following flags can be specified. If none are specified, the Plugin will be
        supplied the compressed video frames: IConsumingDeviceAgent::doPushDataPacket() will
        receive instances of ICompressedVideoPacket. Otherwise,
        IConsumingDeviceAgent::doPushDataPacket() will receive instances of
        IUncompressedVideoFrame.

        - `needUncompressedVideoFrames_yuv420` - This is the most effective video format, because
            it is used internally by the Server, and thus no video frame decoding is needed.
        - `needUncompressedVideoFrames_argb`
        - `needUncompressedVideoFrames_abgr`
        - `needUncompressedVideoFrames_rgba`
        - `needUncompressedVideoFrames_bgra`
        - `needUncompressedVideoFrames_rgb`
        - `needUncompressedVideoFrames_bgr`

    - `deviceDependent` - If set, influences the certain aspects of handling of the Plugin. It is
        intended for Plugins which "work on a device", that is, are wrappers for the video
        analytics running inside the camera. For example, such Plugins are compatible with a
        certain camera family, and typically do not consume the video stream from the Server. Also,
        Device Agents of such Plugins do not have the "Enable/Disable" switch for the user - they
        are always enabled, thus, start working every time a compatible device appears on the
        Server. The opposite Plugin type, not having this flag, is called "device-independent" -
        for example, such Plugins may analyze video from any camera using either their own code, or
        via some backend (a server or an analytics device).

    - `keepObjectBoundingBoxRotation` - When a camera for which the Plugin is working has frame
        rotation option set to 90, 180 or 270 degrees, the Plugin which requests uncompressed video
        frames receives them rotated accordingly. Regardless of whether the plugin requests
        compressed or uncompressed video, the Object Metadata rectangles produced by the Plugin are
        rotated in the reverse direction, because the Server database requires them in the
        coordinate space of a physical (non-rotated) frame. But in the case the analysis is
        performed inside a camera, or the Plugin requests compressed frames (thus, receives them
        not being rotated), it is more convenient for the Plugin to produce Object Metadata in the
        coordinate space of a physical (non-rotated) frame. This flag makes it possible - if set,
        the Server will not perform reverse rotation of the Object Metadata even if the frame
        rotation option on a camera is set.

    Optional; default value is empty.

- `"streamTypeFilter"`: Flag set (String)

    A combination of zero or more of the following flags, separated with `|`, defining which kind
    of streaming data will the plugin receive from the Server in
    IConsumingDeviceAgent::doPushDataPacket():

    - `compressedVideo` - Compressed video packets, as ICompressedVideoPacket.
    - `uncompressedVideo` - Uncompressed video frames, as IUncompressedVideoFrame.
    - `metadata` - Metadata that comes in the stream (e.g. RTSP) that goes from the Device to the
        Server, as ICustomMetadataPacket.
    - `motion` - Motion metadata retrieved by the Server's Motion Engine, as IMotionMetadataPacket.

    Optional; default value is empty.

- `"preferredStream"`: Enumeration (String)

    For plugins consuming a video stream, declares which of the video streams from a camera is
    preferred by the plugin: a low-resolution one, or a high-resolution one.

    - `"undefined"` - VMS choses the stream automatically.
    - `"primary"` - High-resolution stream.
    - `"secondary"` - Low-resolution stream.

    Optional; default value is `"undefined"`.

- `"typeLibrary"`: Object

    Types of Events, Objects and their Attributes that can potentially be produced by any
    DeviceAgent of this Engine. To enable these types, a particular DeviceAgent must whitelist them
    in its Manifest.

    The structure of this JSON object is described in the dedicated document
    [src/nx/sdk/analytics/taxonomy.md](@ref md_src_nx_sdk_analytics_taxonomy).

- `"objectActions"`: Array<Object>

    A possibly empty list of Objects, each being a declaration of an Engine ObjectAction - the user
    may select an Analytics Object (e.g. as a context action for the object rectangle on a video
    frame or in the notification panel), and choose an ObjectAction to trigger from the list of all
    compatible ObjectActions from all Engines.

    When the user triggers an Action, the IEngine::executeAction() is called and supplied the data
    for the Action in the form of IAction.

    This JSON object has the following fields:

    - `"id"`: Id (String)

        Id of the action type, like `"vendor.pluginName.actionName"`.

    - `"name"`: String

        Action name to be shown to the user.

    - `"supportedObjectTypeIds"`: Array<String>

        List of ObjectType ids that are compatible with this Action. An empty list means that the
        Action supports any type of Objects.

    - `"parametersModel"`: SettingsModel (Object)

        Describes the names, types, and default values of the parameters that the user must supply
        before executing the Action.

    - `"requirements"`: Object

        Information about the Action that the Server needs to know. A JSON object with the
        following fields:

        - `"capabilities"`: Flag set (String)

            A combination of zero or more of the following flags, separated with `|`:

            - `needBestShotVideoFrame` - Whether the Action requires the Server to provide it with
                the Best Shot video frame via IAction::getObjectTrackInfo(). If this capability is
                not set, then IObjectTrackInfo::bestShotVideoFrame() will return null to the
                plugin.

            - `needBestShotImage` - Whether the Action requires the Server to provide it with the
                Best Shot image, Best Shot image data size and Best Shot image data format via
                IAction::getObjectTrackInfo(). If this capability is not set, then
                IObjectTrackInfo::bestShotImageData() will return null to the plugin,
                IObjectTrackInfo::bestShotImageDataFormat() will return an empty string to the
                plugin, and IObjectTrackInfo::bestShotImageDataSize() will return 0 to the plugin.

            - `needBestShotObjectMetadata` - Whether the Action requires the Server to provide it
                with the Best Shot rectangle via IAction::getObjectTrackInfo(). If this capability
                is not set, then IObjectTrackInfo::bestShotObjectMetadata() will return null to the
                plugin.

            - `needFullTrack` - Whether the Action requires the Server to provide it with the full
                Object track via IAction::getObjectTrackInfo(). If this capability is not set,
                then IObjectTrackInfo::track() will return null to the plugin, but the
                Server performance on executing the action will be much better, because there will
                be no need to retrieve all Object Metadata (rectangles) for the given Track Id from
                the database.

            Optional; default value is empty.

        - `"bestShotVideoFramePixelFormat"`: Enumeration (String)

            If the `"capabilities"` field includes `needBestShotVideoFrame`, this field must be
            present and have one of the following:
            - `"yuv420"`
            - `"argb"`
            - `"abgr"`
            - `"rgba"`
            - `"bgra"`
            - `"rgb"`
            - `"bgr"`

- `"deviceAgentSettingsModel"`: SettingsModel (Object)

    Describes the names, types, and default values of the settings that a DeviceAgent of this
    plugin may have.

    Optional. If omitted or empty, means there are no settings.

---------------------------------------------------------------------------------------------------
## DeviceAgent Manifest

DeviceAgent Manifest is a JSON Object containing the following fields:

- `"capabilities"`: Flag set (String)

    A combination of zero or more of the following flags, separated with `|`:

    - `disableStreamSelection` - If set, the user will not be offered to choose between the Primary
        (high-quality) and Secondary (low-quality) streams when activating this plugin for a
        camera.

    Optional; default value is empty.

- `"supportedTypes"`: Array<Object>

    Whitelist (filter) of Event and Object types for types declared in this DeviceAgent manifest,
    or in the owner Engine manifest, or in the imported libraries. Contains a list of JSON objects
    with the following fields:

    - `"objectTypeId"` or `"eventTypeId"`: String

    - `"attributes"`: Array<String>

        List of those Attributes of the specified type, which are supported (i.e. are expected to
        be generated) by this DeviceAgent.

        ATTENTION: If the list is empty, it means that this DeviceAgent does not generate any
        Attributes for this type. There is intentionally no way to specify "all Attributes" here -
        each of them must be listed explicitly. This helps backwards compatibility, if the type
        is extended in the future.

    NOTE: Technically, the Plugin can generate types and Attributes not listed in
    `"supportedTypes"`, but in such case the GUI usability or convenience can be affected, like the
    ability to search for such Objects or by such Attributes.

- `"typeLibrary"`: Object

    Types of Events, Objects and their Attributes that can be generated by this particular
    DeviceAgent instance or be used as an Attribute subtype. If an Event or Object of some type
    from this section can be generated by the DeviceAgent directly as an Object or Event (not as an
    aggregated Attribute of a compound Object), it has to be whitelisted in the `"supportedTypes"`
    section to make VMS know how to properly show it in the GUI.

    The structure of this JSON object is described in the dedicated document
    [src/nx/sdk/analytics/taxonomy.md](@ref md_src_nx_sdk_analytics_taxonomy).
