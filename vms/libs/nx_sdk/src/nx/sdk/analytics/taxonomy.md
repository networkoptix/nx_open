# Analytics Taxonomy

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This document describes the mechanism for defining Event and Object types and their Attributes.
Such definitions are made in `"typeLibrary"` field in the Engine or DeviceAgent JSON manifest; for
details of those manifests see
[src/nx/sdk/analytics/manifests.md](@ref md_src_nx_sdk_analytics_manifests).

In the VMS there is a number of pre-defined types which can be used by Plugins either directly, or
via inheriting and extending. Such types are called Taxonomy Base Type Library; their definition
can be found in the file `src/nx/sdk/analytics/taxonomy_base_type_library.json` supplied with the
SDK.

The example of defining a Type Library, as well as using the Base Library, can be found in Stub
Analytics Plugin, "Taxonomy features" sub-plugin, located in this SDK:
samples/stub_analytics_plugin/src/nx/vms_server_plugins/analytics/stub/taxonomy_features/device_agent_manifest.h

---------------------------------------------------------------------------------------------------
## General information

- Error processing:
    - When a Library is parsed by the Server, it may produce errors, but the Server recovers in the
        most obvious way (e.g. ignoring a faulty entity) and continues parsing. The errors are
        logged in the regular Server log.

- Attribute values:
    - The pack of Attributes is represented as a string-to-string multi-map.
        - Any Attribute of a scalar type can hold more than one value, so actually every type is a
            set.
            - Empty set is equivalent to the omitted value.
            - Set with a single value is equivalent to a scalar value.
            - Set with more than one value is called an underdetermined value - semantically, it
                may stand for an ambiguously or simultaneously detected properties.
    - Attribute value is technically always a string: for String type, it's the value string as is,
        for other types it's a JSON value.
    - Any Attribute value can be omitted; there is no concept of a default value of an Attribute.
    - Empty string, `null` (for Attributes which values are JSON objects) and `[]` (for Attributes
        which values are JSON lists) are equivalent to the omitted value.

- Identifiers:
    - Used as ids of Enums (not their names, nor their items), Object types, Event types.
    - Contain only Latin letters, digits, minuses, underscores, and braces; must start with a
        letter, digit or an underscore.
    - The period is used to form a domain-style notation, with the recommendation to drop the
        prefix `com.`.
    - If this syntax is broken (including leading/trailing/multiple periods), it is an error; as a
        recovery, the incorrect identifier is accepted as is.
    - If some entity with an identifier is defined more than once in the particular Library being
        parsed, all these definitions yield entities with the identical internal representation
        (thus, only syntactical JSON differences are allowed), and then they are treated as the
        same entity (thus, all definitions except one are ignored). If some definitions with the
        same id in the same library differ in the internal representation, it is an error.

- Names:
    - Used for names of Attributes, Object types, Event types, Enums, Enum items.
    - Contain only ASCII chars 33..126, Unicode chars >=128 (non-printables are discouraged but not
        checked), spaces can occur only inside and no more than one in a row.
    - If this syntax is broken, it is an error; as a recovery, illegal chars (including spaces) are
        stripped away.

- If an Attribute value is sent from the Plugin which violates some restriction (e.g. a Number type
    hint is violated, or an Enum is assigned a value outside of its items), the Server still
    accepts the value (as a string).

- Icons: Object Types and Event Types can have an icon assigned from the list of pre-defined icons,
    by icon id.

---------------------------------------------------------------------------------------------------
## JSON structure

A Type Library is a JSON object which contain the following lists of entities (in any order):

```
"typeLibrary":
{
    "enumTypes": [ ... ],
    "colorTypes": [ ... ],
    "objectTypes": [ ... ],
    "eventTypes": [ ... ],
    "groups": [ ... ],
    "extendedObjectTypes": [ ... ]
}
```

Each such entity is described in detail below.

### Enum types

This JSON object describes an Enum type which can be used as a type of an Attribute. It has the
following fields:

- `"id"`: Id (String)

    Identifier for the Enum type. May look like `"myCompany.carBodyType"`.

    Mandatory.

- `"name"`: String

    Full name of the Enum type, in English. Will be shown to the user. May look like
    `"Car body type"`.

    Mandatory.

- `"base"`: String

   Optional identifier of an Enum type to inherit items from.

- `"items"`: List<Name (String)>

    List of the enumeration values. Each item is a Name as defined in the "General information"
    section. May look like `[ "Sedan", "Truck or SUV" ]`.

- `"baseItems"`: Array<String>

   Optional whitelist of inherited items. If not empty, only the specified items are inherited,
   other items from the base type(s) are considered not used.

### Color types

This JSON object describes a Color type which can be used as a type of an Attribute. It has the
following fields:

- `"id"`, `"name"`, `"base"`, `"baseItems"`: Similar to the same-name fields of an Enum type.

- `"items"`: List<Object>

    List of color values. Each item is a JSON object with the following fields:

    - `"name"`: Name (String)

        Full name of the color value, in English. Will be shown to the user. May look like
        `"dark"`.

    - `"rgb"`: RGB in HEX format (String)

        The associated RGB value used for this color representation in the UI, for example, when
        the user is presented a palette to choose from when searching. This RGB value is not
        intended to have any direct connection to the color or a real-life object detected on the
        camera. It is assumed that Plugins detect colors approximately, and the attribute value is
        more of a major color class (like the ones defined in natural languages when we talk about
        a "black car" or a "red car") rather than an exact color value extracted from the video
        frame. May look like `"#FF00FF"`.

### Object types

This JSON object describes an Object type. It has the following fields:

- `"id"`: Id (String)

    Identifier for the Object type. May look like `"myCompany.faceDetection.face"`.

    Mandatory.

- `"name"`: String

    Full name of the Object type, in English. Will be shown to the user. May look like
    `"Human face"`.

    Mandatory.

- `"provider"`: String

    Describes the source of the Object type when necessary. For example, if the analysis is
    performed inside a camera, and there are various apps installed on the camera, the name of the
    app which produces such Objects may go here.

    Optional; default value is empty.

- `"icon"`: String

    Identifier of an icon from the icon library built into the VMS. The icons are taken from the
    open-source icon collection "IconPark", v1.1.1. The icon identifier has the form
    `bytedance.iconpark.<name>`. For example, `"bytedance.iconpark.palace"`. The icons can be
    browsed on the official website of the icon collection: http://iconpark.bytedance.com/. The
    icon files can be downloaded from the official open-source repository of the icon collection:
    https://github.com/bytedance/IconPark/.

    NOTE: The icons from the above mentioned icon collection that refer to various brands are not
    available for the VMS Taxonomy.

- `"color"`: String

    Optional. Defines the color of a bounding box used to show such Objects on the video. Can be
    one of the following fixed values:
    - `"Magenta"`
    - `"Blue"`
    - `"Green"`
    - `"Yellow"`
    - `"Cyan"`
    - `"Purple"`
    - `"Orange"`
    - `"Red"`
    - `"White"`

    NOTE: This concept of color has no relation to the one used as an Attribute type.

- `"base"`: String

    Optional name of an Object Type to inherit the Attributes from.

- `"flags"`: Flag set (String)

    A combination of zero or more of the following flags, separated with `|`:

    - `hiddenDerivedType` - this Object type will not be offered to the user in the Search GUI, but
        all its Attributes will be appended to the Attribute list of its base type. Can be applied
        to derived Object types only.

    - `liveOnly` - Objects of such types will not be stored in the video archive - they will be
        only visible when watching live video from the camera (and only when the Objects do not
        come too late from the Analytics Plugin), but they will not be visible when playing back
        the archive, and will not appear in the Search results. This can improve performance when a
        lot of Objects are generated by a Plugin.

    - `nonIndexable` - Objects of such types will be stored in the video archive as usual, but will
        not be added to the Search index and thus will not appear in the Search results, though
        they will be visible when watching live video from the camera (and only when the Objects do
        not come too late from the Analytics Plugin), and when playing back the archive. This can
        improve performance when a lot of Objects are generated by a Plugin, and is recommended for
        Objects which represent some kind of telemetry data - the rectangle of such Objects is
        typically zero-sized, and the attributes define the current values of some measurements.

- `"omittedBaseAttributes"`: Array<String>

    Optional list of Attributes from the base type, listing the Attributes which this type is not
    expected to contain.

- `"attributes"`: Array<Object>

   The definitions of Attributes for this Object Type, in addition to those which are inherited
   from the base type (if any). See the detailed description below.

### Event types

This JSON object describes an Event type. It has the following fields:

- `"id"`: Id (String)

    Identifier for the Event type. May look like `"myCompany.faceDetection.loitering"`.

    Mandatory.

- `"name"`: String

    Full name of the Event type, in English. Will be shown to the user. May look like
    `"Loitering"`.

    Mandatory.

- `"flags"`: Flag set (String)

    A combination of zero or more of the following flags, separated with `|`:

    - `stateDependent` - Prolonged event with active and non-active states.
    - `regionDependent` - Event has reference to a region on a video frame.
    - `hidden` - Event type is hidden in the Client.

    Optional; default value is empty.

- `"groupId"`: Id (String)

    Identifier of a Group that the Event type belongs to.

    Optional; default value is empty, which means that the Event type does not belong to a Group.

- `"provider"`: String

    Describes the source of the Event type when necessary. For example, if the analysis is
    performed inside a camera, and there are various apps installed on the camera, the name of the
    app which produces such Events may go here.

    Optional; default value is empty.

- `"base"`, `"attributes"`, `"omittedBaseAttributes"`, `"icon"`:

    Similar to the same-name fields of an Object type.

### Groups

This JSON object describes a Group for Event types. The particular Group is referenced from an
Event Type via its `"groupId"` field. The Group definition has the following fields:

- `"id"`: Id (String)

    Identifier for the group. May look like `"myCompany.faceDetection.loitering"`. There is no
    problem if such identifier coincides with an identifier of some other entity, e.g. an Event
    type.

    Mandatory.

- `"name"`: String

    Full name of the group, in English. Will be shown to the user. May look like
    `"Loitering-related events"`.

    Mandatory.

### Extended Object Types

This section provides an alternative simplified syntax of defining "hidden" derived Object Types.
Each entity in the `"extendedObjectTypes"` array is technically a definition of a "hidden" derived
Object Type with an auto-generated id using the template `"<pluginId>$<baseObjectTypeId>"`. The `"id"`
field of an Extended Object Type is an id of the base Object Type. The name of the derived Type must
be empty. The Extended Object Type definition has the following fields:

- `"id"`: Id (String)

    Identifier of the base Object Type that is being extended.

- `"attributes"`: Array<Object>

    List of Attributes of the newly defined "hidden" descendant.

### Attribute Lists

Describes a list of Type Attributes, which can be used in the Type definition. Each List must have
an id, and a list of Attributes which uses that same syntax as used in specifying Attributes in
Object and Event Types. Semantically an Attribute List is like a macro - it can be used instead of
an Attribute definition in any Object Type or Event Type definition; its contents simply substitute
the invocation. The Attribute List definition has the following fields:

- `"id"`: Id (String)

    Identifier of the Attribute List.

- `"attributes"`: Array<Object>

    List of Attributes.

---------------------------------------------------------------------------------------------------
## Attributes

Objects and Events can have a list of Attributes, each Attribute being defined with a JSON object
containing the following fields:

- `"name"`: String

    A valid Name as defined in the "General information" section.

- `"type"`: String

    Must have one of the values listed below in the "Attribute types" section.

- `"subtype"`: String

    User-defined id of the particular type. Valid only for user-defined types like `"Enum"`,
    `"Color"`, `"Object"`.

- `"attributeList"`: String
    Id of an Attribute List. If present, all other fields are ignored.

- `"condition"`: String
    Condition string that defines whether this Attribute makes sense for the Object or Event Type
    depending on values of the other Attributes. Uses the same syntax as in the Object Search panel.

Other fields depend on the particular attribute type.

Attributes that are inherited from the base type can be "re-defined" as follows:

- `"subtype"` and `"unit"` can be specified if not specified in the base type (or had exactly the
    same value);

- `"minValue"` and `"maxValue"` can be specified if and only if the inherited range rather than
    extend it.

- For Enums and Colors, the Attribute can be re-defined to use another Enum/Color
    respectively, provided that this overriding Enum/Color is in turn inherited from the
    Enum/Color of the base Attribute.

### Overriden Attributes

The following behavior is observed in the GUI when several types inherited from the same base type
override the same Attribute and declare it as a supported one.

- For Numeric attributes:
    - The widest possible range is applied.
    - If units don't have the same value, the resulting unit is set to an empty string.
    - If at least one Attribute has the `"float"` subtype, the resulting Attribute will be float as
        well.

- For Enum and Color Attributes:
    - The resulting Attribute will contain all the values from the initial Attributes.

- For Object Attributes (aggregated Objects):
    - The resulting Attribute will contain all the sub-attributes that are declared as supported in
        all the types that override the Attribute.
    - All collisions are resolved recursively according to the rules described in this section.

- Boolean and String Attributes stay intact.

- For other type combinations:
    - All such Attributes (though with the same name) are shown in the Search filter GUI, treating
        all the values entered by the user as required criteria, so it makes sense for the user to
        fill only one of them as a filter.

### Attribute types

- `"Number"`:
    - Can hold both integer and floating-point values, as in JSON.
    - Hints: `"minValue"`, `"maxValue"`, `"unit"` (for GUI only).
    - `"subtype"` is treated like a hint: can be `"integer"` or `"float"` (default).

- `"Boolean"`:
    - Can be either True, False, or omitted (which is a distinct case).
    - Case-insensitive `"true"`, `"false"` is accepted by the Server from a Plugin, as well as
        `"0"` and `"1"`.

- `"String"`:
    - An empty string is equivalent to the omitted attribute.
    - Cannot be restricted to be non-empty.
    - May contain any Unicode chars, including '\n', '\0' and other control chars.

- `"Enum"`:
    - The set of Enum items can be empty - it makes sense for extending Enums.
    - `"subtype"` in the attribute definition refers to a user-defined Enum type id.
    - Item names are not Identifiers - they are Names.
    - Enums can be inherited and extended via `"base"` and `"baseItems"` fields.

- `"Color"`:
    - Similar to an Enum, but each item has an associated RGB value used for this color
        representation in the UI.
    - `"subtype"` in the attribute definition refers to a user-defined Color type id.
    - The color Name is what appears as the Attbiture value.
    - Like Enums, Colors can be inherited and extended via `"base"` and `"baseItems"` fields.

- `"Object"`:
    - A nested (aggregated) Object of the specified `"subtype"` Object type, or of any type (if
        `"subtype"` is omitted).
    - Can be null, which is equivalent to the omitted attribute.
    - The following rules are used to represent a nested Object in the Attribute values of a
        particular instance of the owner (outer) Object:
        - For each Attribute of an inner Object, an Attribute with the required type and the name
            `<ownerAttributeName>.<innerAttributeName>` defines its value.
        - Additionally, there may be a boolean Attribute with the name `<ownerAttributeName>`.
            - If it equals `true`, the nested Object is considered present.
            - If it equals `false`, the nested Object is considered omitted (null).
            - If it is omitted, the presence of the nested Object is deduced by the presence of any
                of its individual Attributes.
        - NOTE: This scheme allows for an instance that contains a nested Object which has no
            Attribute values, but the Object itself is considered to be present.
