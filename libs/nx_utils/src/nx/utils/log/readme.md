# Logging subsystem {#nx_utils_log}

## Writing logs

TL;DR:
```
#include <nx/utils/log/log.h>

NX_INFO(this, "Application started");
NX_DEBUG(this, "Value %1 is %2", name, value);
NX_VERBOSE(NX_SCOPE_TAG) << "Static method is called with args" << arg1 << arg2;
```

### Formal syntax

Log record can be added using the followind macros:
```
NX_<LEVEL>(<TAG>, <FORMAT>[, <MESSAGE> ... ]);
NX_<LEVEL>(<TAG>) << <MESSAGE> [<< <MESSAGE> ... ];
```

Where:
- **LEVEL** - One of log levels: ALWAYS, ERROR, WARNING, INFO, DEBUG, VERBOSE.
- **TAG** - `nx::utils::log::Tag` object. See [Tags](#tags) for details.
- **MESSAGE** - Any convertible to string object. See [To string](#tostring) for details.

### Deprecated but still supported methods

Standard Qt logging methods( `qDebug()`, `qWarning()`, ...) and old helper methods (`qnDebug()`,
`qnWarning()`, ...) are highly not recommended for usage (though still supported).

### Tags @anchor tags

Tag is a log record entity, used for search and filtering. It is represented by
`nx::utils::log::Tag` class object, which can be automatically constructed from:
- `this` - in this case object's type name with all namespaces and pointer will be in a tag.
- Smart pointers work the same way as `this`.
- `std::type_info` - in this case type name with all namespaces will be in a tag.

`NX_SCOPE_TAG` macro provides well-formed `Tag` for static functions.

`nx::utils::log::Tag` can also be constructed from string explicitly, but this should be avoided in
most cases because type info carries a lot more information in its name and namespace, e.g.
`nx::network::Socket(...)` points to `nx_network` module class `Socket`.

When using logs in static functions the recommended way is to use `NX_SCOPE_TAG` macro:
```
static void logSomething()
{
    NX_INFO(NX_SCOPE_TAG, "Log Something");
}
```

Alternative approach can be used when you need to group similar set of functions under a fixed tag
(to simplify search).
```
namespace nx::vms::utils {

namespace {

struct UtilsLog {};
static const nx::utils::log::Tag kUtilsTag{typeid(UtilsLog)};

} // namespace

void firstUtilFunction()
{
    NX_INFO(kUtilsTag, "Log Something First");
}

void secondUtilFunction()
{
    NX_INFO(kUtilsTag, "Log Something Second");
}

} // namespace nx::vms::utils
```


### To string conversions @anchor tostring

If you want your class to be displayed in logs in some human-readable form, you should implement
one of the following methods:
- `QString toString() const` class method
- `QString toString(const T&)` function, useful for library types and enumerations
- `QDebug operator<<(QDebug)` class method
- `QDebug operator<<(QDebug, const T&)` function, useful for library types and enumerations

All standard types are already supported. Pointers are also supported by default: implementation
includes type name and pointer address. To override this behavior you can implement the following
class method:
- `QString idForToStringFromPtr() const`

Any convertible to string objects can be used as message arguments. Recommended usage is the
following: `NX_DEBUG(this, "Value %1 is %2", name, value)`
Alternatively you can manually create a log message:
`NX_DEBUG(this, lm("Value %1 is %2").args(name, value))`

## Logging configuration and filtering

All log records are dispatched in a single module `nx::utils::log::LoggerCollection`. This
collection stores a number of different **loggers** implementing `nx::utils::log::AbstractLogger`
interface. Logger checks record [**tag**](#tags) (`nx::utils::log::Tag`), and if it is accepted
by its **filters** (`nx::utils::log::Filter`), record is dispatched to one or several
**writers** (`nx::utils::log::AbstractWriter`). Default logger implementation is the
 `nx::utils::log::Logger` class

Loggers can work in *cooperative* or *exclusive* mode. First of all, each exclusive logger checks
the record tag. If it is acceptable, no other loggers receive this record. Otherwise all other
loggers (working in cooperative mode) receive this record, and each one of them can write it.

### Logger configuration

Set of loggers, working in the *cooperative* mode, can be created and configured using the
`nx::utils::log::Settings` class. Each of these loggers can have it's own writer. For now settings
support only two writers:
- `nx::utils::log::StdOut` - writes logs into stdout and stderr.
- `nx::utils::log::File` - writes logs into rotating files.

Special value `-` can be written to the `logBaseName` settings field to initialize stdout writer,
file writer will be used otherwise.

Alternative writers can be assigned manually:
- `nx::utils::log::Buffer` - just keeps logs inside (useful for unit tests).
- `nx::utils::log::NullDevice` - does nothing.
- `TcpLogWriterOut` - writes the logs into a tcp socket.

### Creating and registering logger

Loggers can be created with the helper `nx::utils::log::buildLogger()` function. This function
creates set of loggers, working in the *cooperative* mode, and combines them to a single
`nx::utils::log::AggregateLogger` instance.

Additional function parameters allow to define a header and also provide a set of filters,
which will define the *exclusive* mode for this aggregate logger.

Logger can be registered with `nx::utils::log::setMainLogger()` function or with
`nx::utils::log::addLogger()` function. Only one logger can be marked as main logger. It's
difference from others is actual only for the *exclusive* mode. First, additional loggers
are asked about their exclusive filters, and if no one handles the given log record, it is
passed to main logger.

### Log configuration with file

Logger settings (`nx::utils::log::Settings`) can be created using QSettings engine (ini file).

It should be used by the following way: `nx::utils::log::Settings(QSettings*)`. Supported syntax is
the following:
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

This file declares some general constants and two log outputs: console (`-`) and `client_log.log`
file. Each output can contain keys for every log level. Value is the regular expression, which will
be used for parsing. Default level is declared using the `*` value.

For example, using this config will print to console every `INFO` level message except those
containing `::discovery::` keyword in a tag and so on, and additionally `DEBUG` messages containing
 `MediaPlayer` or `QnRtspClient` keywords in their tags.

### Single-line log configuration

Logger settings (`nx::utils::log::Settings`) can be created using single-line form (options are
limited in terms of regular expressions as braces and comma symbols cannot be used).

`nx::utils::log::Settings::load()` loads parameters from `QnSettings` that represents parsed
application configuration options taken whether from command line arguments, from configuration
file, from Windows registry or from the http handler.

The following parameters are supported:
- log/logger=LOGGER_SETTINGS.
    E.g., `--log/logger=file=/var/log/http,level=VERBOSE[nx::network::http],level=none`
    logs only messages with `nx::network::http` prefix with level <= VERBOSE to /var/log/http_log.
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

