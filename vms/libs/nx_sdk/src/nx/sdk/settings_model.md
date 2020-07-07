# Settings Model

In various places in the SDK, e.g. in Manifests of Analytics Plugin entities, there is a
possibility to define a Settings Model - a JSON describing a hierarchical structure of settings,
each having its name, type, and default value.

The Client can offer a user a dialog with controls corresponding to such Model. When the user fills
the dialog, a set of values is formed, represented as a map of strings, where the key is the id of
the particular setting in the Model, and the value is either a string itself (for settings of
the type String), or a JSON-formatted value (for settings of other types).

TODO: Describe all currently implemented setting types, including ROI, values format, and hierarchy
(repeaters and groups).


