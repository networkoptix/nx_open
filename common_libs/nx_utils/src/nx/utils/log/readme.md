# Logging Subsystem {#nx_utils_log}

## Writing to Logs from Code

```
#include <nx/utils/log/log.h>

NX_<LEVEL>(<TAG>, <MESSAGE>);
NX_<LEVEL>(<TAG>) << <MESSAGE>;
```

Where:

- **LEVEL** - One of log levels: ALWAYS, ERROR, WARNING, INFO, DEBUG, VERBOSE.
- **TAG** - `nx::utils::log::Tag`, normally `this` of current class or `typeid` for static methods.
- **MESSAGE** - Any string like type or type with toString, see "To String" section.

For ditatils see: `nx/utils/log/`

There is still old syntax available, but it should be avoided and replaced and replaced with new
one: `NX_LOG([<ID>,] <MESSAGE>, cl_log<LEVEL>)` and `NX_LOGX` with the same syntax.

There is also a lot of `qDebug() <<` and `qWarning() <<` in the code, which should be replaced with
`NX_DEBUG(this) <<`, and `NX_WARNING(this) <<`.


## Log Tags

`nx::utils::log::Tag` - is a log tag which is automatically constructed from:

- Pointers - in this case object's type name with all namespaces and pointer will be in a tag.
- `std::type_info` - in this case type name with all namespaces will be in a tag.

For more details see `toString` for pointers and `typeinfo`.

`nx::utils::log::Tag` can also be constructed from string explicitly, but this should be avoided in
most cases because type info carres a lot more information in it's name and namespace, e.g.
`nx::network::Socket(...)` points to `nx_network` module class `Socket`.


## To String and Messages

There is a global `::toString` function which works for all types if they:

- Implement `QSting toString() const` function, is is recommended to implement is in your types.
- Implement external `QString toString(const T&)`, useful for library types and `enum class`es.

For details see `nx/utils/log/to_string.h`, all standard types are implemented there.

There is also an implementation for pointers which includes type name and pointer address, you may
implement `QString idForToStringFromPtr() const` in your classes to see more detailed information.

There is also a string formater, which supports all stringable types and works similar to `QString`
build in formater `lm("%1 = %2").args("timeout", std::chrono::hours(1))` = "timeout = 1h".


## Log Configuration for Module

There is a class that helps to configure logging for application `nx::utils::log::Settings`,
it reads configuration from `QnSettings`, so all modules have similar configuration options.
Specific options can be found in NX Wiki.

TODO: #akolesnikov: Describe new configuration.


## Loggers

All logs in application go into global loggers (`nx::utils::log::AbstractLogger` implementations)
which can be added for specific tag prefixes by `nx::utils::log::addLogger`. These loggers can be
accessed later by `nx::utils::log::getLogger`. All other tags go into `nx::utils::log::mainLogger`,
so applications which do not need more than 1 logger may configure only it.

There are 2 main implementations of `nx::utils::log::AbstractLogger`:

- `nx::utils::log::AggregateLogger` - aggregates several loggers into a single one.
- `nx::utils::log::Logger` - passes records which satisfy level rules into a list of
  `nx::utils::log::AbstractWriter` implementations.

There are several default writers:

- `nx::utils::log::StdOut` - writes logs into stdout and stderr.
- `nx::utils::log::File` - writes logs into rotating files.
- `nx::utils::log::Buffer` - just keeps logs inside.
- `nx::utils::log::NullDevice` - does nothing.
