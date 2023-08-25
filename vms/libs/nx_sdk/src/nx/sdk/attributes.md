# Attributes with pre-defined behavior

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This document describes rules and conventions for the usage of Attributes throughout the SDK.

---------------------------------------------------------------------------------------------------
## Attribute names

Generally, an attribute name is chosen by the Plugin, and is directly shown to the user in certain
cases. So, it is recommended to be a human-readable word or word combination in English. There are
though certain recommendations and limitations for the names, listed below.

### Recommendations for Names

Violation of these recommendations for Attribute names may lead to unexpected and/or undesired
behavior in both the VMS GUI and business logic.

- Name must not have leading or trailing spaces.
- Name must not contain control characters (0..31 or 127).
- There must be no more than one consecutive space.

### Special Attributes

Names starting with `nx.` and/or having `.sys.` in the middle are sometimes treated by the VMS as
having a certain special behavior, thus, must not be used when such special behavior is not
intended.

Identifiers starting with `nx.` must not be used for the entities introduced by parties other than
Nx.

Some Attributes with pre-defined names starting with `nx.sys.` are used for the VMS internal
purposes and to access certain, sometimes experimental, features. Some of such Attributes are
described in a dedicated section of this document.

Some Attributes with names having `.sys.` in the middle are treated by the VMS in certain special
ways. Some of such Attributes are described in a dedicated section of this document.

---------------------------------------------------------------------------------------------------
## Attributes with special behavior (`.sys.`)

- Attributes of Analytics Objects which end with `.sys.hidden` do not appear on video in the
    Client. In all other aspects such Attributes are treated regularly, including search.

---------------------------------------------------------------------------------------------------
## Pre-defined Attributes (`nx.sys.`)

Attributes with names starting with `nx.sys.` are treated specially: they do not appear on video
similarly to `*.sys.hidden` Attributes, and the following pre-defined attributes have special
behavior:

- `nx.sys.color`: Such Attribute defines the color of the Object Bounding Box on video in the
Client. Its value can be either of the following:
    - RGB color in form `#RRGGBB`, where `RR`, `GG` and `BB` are hexadecimal values of red, green
        and blue color components respectively.
    - Fixed color name from the following palette (case-sensitive):
        - `Magenta`, `Blue`, `Green`, `Yellow`, `Cyan`, `Purple`, `Orange`, `Red`, `White`.
    - NOTE: Other values are treated as if the Attribute was not specified at all.
