# Logging subsystem {#nx_utils_log}

## Writing to logs from code

```
#include <nx/utils/log/log.h>

NX_<LEVEL>(<TAG>, <FORMAT>[, <ARGS> ... ]);
NX_<LEVEL>(<TAG>) << <MESSAGE> [<< <ARGS> ... ]; // Should only be used to replase qDebug and qWarning.
```

Where:

- **LEVEL** - One of log levels: ALWAYS, ERROR, WARNING, INFO, DEBUG, VERBOSE.
- **TAG** - `nx::utils::log::Tag`, normally `this` or `typeid(ClassName)` for static methods.
- **MESSAGE** - Any string-like object or object with `toString()` support, see "To string" section.

For details see: `nx/utils/log/`

Examples:
```
NX_INFO(this, "Application started");
NX_DEBUG(this, "Value %1 is %2", name, value);
```

There is still an old syntax available: `NX_LOG([<ID>,] <MESSAGE>, cl_log<LEVEL>)` and `NX_LOGX`,
but it should be avoided and replaced with new one.

There is also a lot of `qDebug() <<` and `qWarning() <<` in the code, which should be replaced with
`NX_DEBUG(this) <<`, and `NX_WARNING(this) <<`.


## Log tags

`nx::utils::log::Tag` - is a log tag which is automatically constructed from:

- `this` - in this case object's type name with all namespaces and pointer will be in a tag.
- Smart pointers work the same way as `this`.
- `std::type_info` - in this case type name with all namespaces will be in a tag.

For more details see `toString()` for pointers and `std::type_info`.

`nx::utils::log::Tag` can also be constructed from string explicitly, but this should be avoided in
most cases because type info carries a lot more information in its name and namespace, e.g.
`nx::network::Socket(...)` points to `nx_network` module class `Socket`.

When using logs in standalone functions, recommended way to tag logs is to create a dummy object:
```
namespace nx::vms::utils {

namespace {

struct UtilsLog {};
static const nx::utils::log::Tag kUtilsTag{typeid(UtilsLog)};

} // namespace

void logSomething()
{
    NX_INFO(kUtilsTag, "Log Something");
}

} // namespace nx::vms::utils
```


## To string convertion and messages

There is a global `::toString` function which works for all types if they:

- Implement `QSting toString() const` function, is is recommended to implement is in your types.
- Implement external `QString toString(const T&)`, useful for library types and `enum class`es.

For details see `nx/utils/log/to_string.h`, all standard types are implemented there.

There is also an implementation for pointers which includes type name and pointer address, you may
implement `QString idForToStringFromPtr() const` in your classes to see more detailed information.

There is also a string formater, which supports all stringable types and works similar to `QString`
build in formater: `nx::utils::log::Message("%1 = %2").args("timeout", std::chrono::hours(1))`
will produce "timeout = 1h". The same syntax is build in `NX_<LEVEL>(<TAG>, <FORMAT>, <ARGS> ...)`
so it should be used rather than `NX_<LEVEL>(<TAG>, lm(<FORMAT>).args(<ARGS> ...))`.

## Loggers

All logs in the application go into global loggers (`nx::utils::log::AbstractLogger` interface),
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

## Log configuration for module

Loggers are built using `nx::utils::log::buildLogger()` function.
The function accepts `nx::utils::log::Settings` that specify desired log filters.
`nx::utils::log::Settings::load()` loads parameters from `QnSettings` that represents
parsed application configuration options taken whether from command line arguments
or conf file (Linux/Max) / registry (Mswin).

The following parameters are supported:
- log/logger=LOGGER_SETTINGS.
    E.g., "--log/logger=file=/var/log/http,level=VERBOSE[nx::network::http],level=none"
    logs only messages with nx::network::http prefix with level <= VERBOSE to /var/log/http_log.
    For full description of LOGGER_SETTINGS and more examples see `LoggerSettings::parse`.

- log-file=path/to/file
- log-level=LogLevel (log/logLevel=LogLevel)
- log/maxFileSize={Maximum log file size in bytes}.
    When this size is reached, log file is archived and new file is created.
- log/maxBackupCount={Maximum number of log files to keep}

When using as command-line argument each parameter is prefixed with --. E.g., --log/logger=...
When using a configuration file (`nx::utils::log::Settings::load(...) `) , then log/logger value should be specified as:
```
[log]
logger=...
```

Alternative human-readable scheme is implemented using `QSettings` engine. It should be used by the following way: `nx::utils::log::Settings(QSettings*)`. Supported syntax is the following:
```
[General]
logArchiveSize=10
maxLogFileSize=10485760

[-]
debug=MediaPlayer|QnRtspClient
none=::discovery::|^nx::network::|::time_sync::
info=*

[client_log]
none=::discovery::|nx::network::
debug=*
```

This file declares some general constants and two log outputs: console (`-`) and `client_log.log` file. Each output can contain keys for every log level. Value is the regular expression, which will be used for parsing.
Default level is declared using the `*` value.

For example, using this config will print to console every `INFO` level message except those containing `::discovery::` keyword in a tag and so on, and additionally `DEBUG` messages containing `MediaPlayer` or `QnRtspClient` keywords in their tags.
