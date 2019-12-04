#include "cli_options.h"

#include <string>
#include <iostream>

#include <QFileInfo>

#include <nx/kit/utils.h>
#include <nx/utils/log/assert.h>

#include <nx/vms/testcamera/test_camera_ini.h>

namespace nx::vms::testcamera {

void showHelp(const char* exeName)
{
    if (!NX_ASSERT(exeName) || !NX_ASSERT(exeName[0]))
        return;

    const std::string baseExeName = QFileInfo(exeName).baseName().toStdString();

    std::cout << /*suppress newline*/1 + R"help(
Usage:
 )help" + baseExeName + R"help( [<option> | <cameraSet>]...

At least one <cameraSet> is required; it is a concatenation of semicolon-separated parameters:
 count=<numberOfCameras>
    Optional. Number of cameras to start, if not --camera-for-file. Default: 1.
 files=<filename>[,<filename>]...
     Files for the primary stream. The part after `=` may be enclosed in `"` or `'`.
 secondary-files=<filename>[,<filename>]...
     Optional. Files for the secondary stream. The part after `=` may be enclosed in `"` or `'`.
 offline=0..100
     Optional. Frequency of the Camera going offline. Default: 0 - never go offline.

<option> is one of the following:
 -h, --help
     Show this help text and exit.
 -L, --log-level[=]<level>
     Log level: one of NONE, ERROR, WARNING, INFO, DEBUG, VERBOSE; not case-sensitive, and can be
     shortened to the first letter: N, E, W, I, D, V. Default: INFO
 --max-file-size-megabytes[=]<value>
     Load no more than this size of each video file. 0 means no limit. Default: 100.
 -I, --local-interface[=]<ipAddressOrHostname>
     Local interface to listen. Can be specified multiple times to form a list. By default, all
     interfaces are listened to.
 -S, --camera-for-file
     Run a separate Camera for each primary file. If specified, the 'count' parameter must be
     omitted, and if not --no-secondary, the number of primary and secondary files must be the
     same.
 --pts
     Include PTS values from the video file into the stream. If omitted, ignore those PTS values.
 --shift-pts-us[=]<ptsAbsoluteIncrement>
     Shift PTS axis for all streams and cameras so that 0 becomes the specified value (which can be
     negative). Such shift is performed after the possible PTS "unlooping", and cannot be requested
     if any other pts shifting is requested.
 --unloop-pts
     "Unloop" PTS sequence to be monotonous - after the last PTS is processed, the next one will
     differ from it by the difference between the last PTS and the one before it. May not work well
     with files containing B-frames (reordered frames). Can be specified only when each Camera
     streams one file.
 --shift-pts-from-now-us[=]<ptsRelativeIncrement>
     Shift PTS axis for all streams and cameras so that 0 becomes the current time plus the
     specified value (which can be negative). Such shift is performed after PTS "unlooping", thus
     requires --unloop-pts, and cannot be specified if any other pts shifting is requested.
 --shift-pts-primary-period-us[=]<period>
 --shift-pts-secondary-period-us[=]<period>
     Shift PTS axis so that 0 becomes `(now / period - 1) * period`.
     Cannot be specified without --unloop-pts or if any other PTS shifting is requested.
     Makes `unloopedPts % period == originalPts`, which allows to identify video frames by PTS.
     The period value for a file can be learned from the console when running with --unloop-pts.
 --no-secondary
     Do not stream the secondary stream.
 --fps[=]<value>
     Force FPS for both primary and secondary streams to the given positive integer value.
 --fps-primary[=]<value>
     Force FPS for the primary stream to the given positive integer value.
 --fps-secondary[=]<value>
     Force FPS for the secondary stream to the given positive integer value.

Example:
 )help" + baseExeName + R"help( files=c:/test.264;count=20
)help";
}

namespace {

static QString enquoteAndEscape(const QString& value)
{
    return QString::fromStdString(nx::kit::utils::toString(value.toStdString()));
}

static QString unquote(const QString& value)
{
    QString result = value;

    if (result.startsWith("\"") || result.startsWith("'"))
        result = result.mid(1);

    if (result.endsWith("\"") || result.endsWith("'"))
        result = result.left(result.length() - 1);

    return result;
}

class InvalidArgs: public std::exception
{
public:
    InvalidArgs(const QString& message): m_message(message.toStdString()) {}
    virtual const char* what() const noexcept override { return m_message.c_str(); }

private:
    const std::string m_message;
};

/** @throws InvalidArgs */
static int positiveIntArg(const QString& argName, const QString& argValue)
{
    const int value = argValue.toInt(); //< Returns 0 on failure.
    if (value <= 0)
    {
        throw InvalidArgs(
            "Invalid value (expected positive 32-bit integer) for arg '" + argName + "': "
                + enquoteAndEscape(argValue) + ".");
    }
    return value;
}

/** @throws InvalidArgs */
static int64_t nonNegativeIntArg(const QString& argName, const QString& argValue)
{
    bool ok = false;
    const int64_t value = argValue.toInt(&ok);
    if (!ok || value < 0)
    {
        throw InvalidArgs(
            "Invalid value (expected non-negative 32-bit integer) for arg '" + argName + "': "
                + enquoteAndEscape(argValue) + ".");
    }
    return value;
}

/** @throws InvalidArgs */
static std::chrono::microseconds positiveUsArg(const QString& argName, const QString& argValue)
{
    const int64_t intValue = argValue.toLongLong(); //< Returns 0 on failure.
    if (intValue <= 0)
    {
        throw InvalidArgs(
            "Invalid value (expected positive integer) for arg '" + argName + "': "
                + enquoteAndEscape(argValue) + ".");
    }
    return std::chrono::microseconds{intValue};
}

/** @throws InvalidArgs */
static std::optional<std::chrono::microseconds> usArg(
    const QString& argName, const QString& argValue)
{
    bool ok = false;
    const int64_t intValue = argValue.toLongLong(&ok);
    if (!ok)
    {
        throw InvalidArgs(
            "Invalid value (expected integer) for arg '" + argName + "': "
                + enquoteAndEscape(argValue) + ".");
    }
    return std::chrono::microseconds(intValue);
}

/** @throws InvalidArgs */
static QString nonEmptyStringArg(const QString& argName, const QString& argValue)
{
    if (argValue.isEmpty())
        throw InvalidArgs("Empty value for arg '" + argName + "' not allowed.");
    return argValue;
}

/** @throws InvalidArgs */
static nx::utils::log::Level logLevelArg(const QString& argName, const QString& argValue)
{
    const nx::utils::log::Level logLevel = nx::utils::log::levelFromString(argValue);

    if (logLevel == nx::utils::log::Level::undefined
        || logLevel == nx::utils::log::Level::notConfigured)
    {
        throw InvalidArgs("Invalid log-level value in arg '" + argName + "': "
            + enquoteAndEscape(argValue) + ".");
    }
    return logLevel;
}

/**
 * @throws InvalidArgs
 */
static int parseIntCameraSetParameter(
    const QString& name, const QString& value, int min, int max)
{
    bool success = false;

    const int result = value.toInt(&success);

    if (!success)
        throw InvalidArgs("Parameter '" + name + "=' must be an integer.");
    if (result < min)
        throw InvalidArgs("Parameter '" + name + "=' must be >= " + QString::number(min) + ".");
    if (result > max)
        throw InvalidArgs("Parameter '" + name + "=' must be <= " + QString::number(max) + ".");

    return result;
}

/** Intended for debug. A valid JSON is not guaranteed. */
static QString cameraSetToJsonString(const CliOptions::CameraSet& cameraSet, const QString& prefix)
{
    QString result;

    result += prefix + "{\n";

    result += prefix + "    \"offlineFreq\": " + QString::number(cameraSet.offlineFreq) + ",\n";

    result += prefix + "    \"count\": " + QString::number(cameraSet.count) + ",\n";

    result += prefix + "    \"primaryFileNames\":\n";
    result += prefix + "    [\n";
    for (int i = 0; i < cameraSet.primaryFileNames.size(); ++i)
    {
        result += prefix + "        " + enquoteAndEscape(cameraSet.primaryFileNames[i]);
        result += (i == cameraSet.primaryFileNames.size() - 1) ? "" : ",";
        result += "\n";
    }
    result += prefix + "    ],\n";

    result += prefix + "    \"secondaryFileNames\":\n";
    result += prefix + "    [\n";
    for (int i = 0; i < cameraSet.secondaryFileNames.size(); ++i)
    {
        result += prefix + "        " + enquoteAndEscape(cameraSet.secondaryFileNames[i]);
        result += (i == cameraSet.secondaryFileNames.size() - 1) ? "" : ",";
        result += "\n";
    }
    result += prefix + "    ]\n";

    result += prefix + "}";

    return result;
}

static QString boolToJson(bool value)
{
    return value ? "true" : "false";
}

static QString optionalIntToJson(std::optional<int> value)
{
    if (!value)
        return "null";
    return QString::number(*value);
}

static QString optionalUsToJson(std::optional<std::chrono::microseconds> value)
{
    if (!value)
        return "null";
    return lm("\"%1 us\"").args(value->count()); //< Make a JSON value of type "string".
}


/** Intended for debug. A valid JSON is not guaranteed. */
static QString optionsToJsonString(const CliOptions& options)
{
    QString result = "{\n";

    result += "    \"showHelp\": " + boolToJson(options.showHelp) + ",\n";
    result += "    \"logLevel\": " + lm("\"%1\"").args(options.logLevel) + ",\n";
    result += "    \"maxFileSizeMegabytes\": "
        + QString::number(options.maxFileSizeMegabytes) + ",\n";
    result += "    \"cameraForFile\": " + boolToJson(options.cameraForFile) + ",\n";
    result += "    \"includePts\": " + boolToJson(options.includePts) + ",\n";
    result += "    \"shiftPts\": " + optionalUsToJson(options.shiftPts) + ",\n";
    result += "    \"noSecondary\": " + boolToJson(options.noSecondary) + ",\n";
    result += "    \"fpsPrimary\": " + optionalIntToJson(options.fpsPrimary) + ",\n";
    result += "    \"fpsSecondary\": " + optionalIntToJson(options.fpsSecondary) + ",\n";
    result += "    \"unloopPts\": " + boolToJson(options.unloopPts) + ",\n";
    result += "    \"shiftPtsFromNow\": " + optionalUsToJson(options.shiftPtsFromNow) + ",\n";
    result += "    \"shiftPtsPrimaryPeriod\": "
        + optionalUsToJson(options.shiftPtsPrimaryPeriod) + ",\n";
    result += "    \"shiftPtsSecondaryPeriod\": "
        + optionalUsToJson(options.shiftPtsSecondaryPeriod) + ",\n";

    result += "    \"localInterfaces\":\n";
    result += "    [\n";
    for (int i = 0; i < (int) options.localInterfaces.size(); ++i)
    {
        result += "        " + enquoteAndEscape(options.localInterfaces[i]);
        result += (i == options.localInterfaces.size() - 1) ? "" : ",";
        result += "\n";
    }
    result += "    ],\n";

    result += "    \"cameraSets\":\n";
    result += "    [\n";
    for (int i = 0; i < (int) options.cameraSets.size(); ++i)
    {
        const auto& cameraSet = options.cameraSets.at(i);
        result += cameraSetToJsonString(cameraSet, /*prefix*/ QString(8, ' '));
        result += (i == (int) options.cameraSets.size() - 1) ? "" : ",";
        result += "\n";
    }
    result += "    ]\n";

    result += "}";
    return result;
}

/** @throws InvalidArgs */
static CliOptions::CameraSet parseCameraSet(const QString& arg)
{
    CliOptions::CameraSet cameraSet;
    const QStringList parameters = arg.split(';');
    for (const auto& parameter: parameters)
    {
        const QStringList parts = parameter.split('=');
        if (parts.size() != 2)
            throw InvalidArgs("Invalid parameter: " + enquoteAndEscape(parameter) + ".");
        const QString name = parts[0].toLower().trimmed();
        const QString& value = parts[1];
        if (name == "offlineFreq")
            cameraSet.offlineFreq = parseIntCameraSetParameter(name, value, 0, 100);
        else if (name == "count")
            cameraSet.count = parseIntCameraSetParameter(name, value, 1, INT_MAX);
        else if (name == "files")
            cameraSet.primaryFileNames = unquote(value).split(',');
        else if (name == "secondary-files")
            cameraSet.secondaryFileNames = unquote(value).split(',');
        else
            throw InvalidArgs("Unknown parameter '" + name + "='.");
    }

    if (cameraSet.primaryFileNames.isEmpty())
        throw InvalidArgs("Parameter 'files=' must be specified.");

    return cameraSet;
}

static QString valueAfterEquals(const QString& arg)
{
    return arg.mid(arg.indexOf('=') + 1);
}

/**
 * @param argv Must end with null, as argv[] from main() does.
 * @throws InvalidArgs
 */
static QString nextArg(const char* const argv[], int* argp)
{
    if (argv[*argp + 1] == nullptr)
        throw InvalidArgs("Missing value after arg '" + QString(argv[*argp]) + "'.");

    ++(*argp);
    return QString(argv[*argp]);
}

/** @throws InvalidArgs */
static QString validatedArg(
    const char* const argv[],
    int* argp,
    const QString& longArgName,
    const QString& shortArgName = QString())
{
    if (!NX_ASSERT(!longArgName.isEmpty()) || !NX_ASSERT(longArgName.startsWith("--"))
        || !NX_ASSERT(shortArgName.isEmpty() || shortArgName.startsWith("-")))
    {
        throw InvalidArgs(lm("INTERNAL ERROR: Unable to parse arg: long name %1, short name %2.")
            .args(enquoteAndEscape(longArgName), enquoteAndEscape(shortArgName)));
    }

    const QString arg = argv[*argp];

    if (arg.isEmpty())
        throw InvalidArgs(lm("Command-line arg #%1 is empty.").args(*argp));

    return arg;
}

/** @throws InvalidArgs */
template<typename ParseArgFunc>
std::optional<std::invoke_result_t<ParseArgFunc, const QString&, const QString&>> parse(
    const char* const argv[],
    int* argp,
    ParseArgFunc parseArgFunc,
    const QString& longArgName,
    const QString& shortArgName = QString())
{
    const QString arg = validatedArg(argv, argp, longArgName, shortArgName);

    if (arg.startsWith(longArgName + "=")
        || (!shortArgName.isEmpty() && arg.startsWith(shortArgName + "=")))
    {
        return parseArgFunc(arg, valueAfterEquals(arg));
    }

    if (arg == longArgName || arg == shortArgName)
        return parseArgFunc(arg, nextArg(argv, argp));

    return std::nullopt;
}

/**
 * @throws InvalidArgs
 * @return Either nullopt, or true.
 */
static std::optional<bool> parseFlag(
    const char* const argv[],
    int* argp,
    const QString& longArgName,
    const QString& shortArgName = QString())
{
    const QString arg = validatedArg(argv, argp, longArgName, shortArgName);
    if (arg == longArgName || arg == shortArgName)
        return true;

    return std::nullopt;
}

/** @throws InvalidArgs */
static void parseOption(
    CliOptions* options, const char* const argv[], int* argp, std::optional<int>* outFps)
{
    const QString arg = argv[*argp];

    if (const auto v = parseFlag(argv, argp, "--help", "-h"))
        options->showHelp = *v;
    else if (const auto v = parse(argv, argp, nonEmptyStringArg, "--local-interface", "-I"))
        options->localInterfaces.push_back(*v);
    else if (const auto v = parse(argv, argp, positiveIntArg, "--fps"))
        *outFps = *v;
    else if (const auto v = parse(argv, argp, positiveIntArg, "--fps-primary"))
        options->fpsPrimary = *v;
    else if (const auto v = parse(argv, argp, positiveIntArg, "--fps-secondary"))
        options->fpsSecondary = *v;
    else if (const auto v = parseFlag(argv, argp, "--camera-for-file", "-S"))
        options->cameraForFile = *v;
    else if (const auto v = parseFlag(argv, argp, "--no-secondary"))
        options->noSecondary = *v;
    else if (const auto v = parseFlag(argv, argp, "--pts"))
        options->includePts = *v;
    else if (const auto v = parse(argv, argp, usArg, "--shift-pts-us"))
        options->shiftPts = *v;
    else if (const auto v = parseFlag(argv, argp, "--unloop-pts"))
        options->unloopPts = *v;
    else if (const auto v = parse(argv, argp, usArg, "--shift-pts-from-now-us"))
        options->shiftPtsFromNow = *v;
    else if (const auto v = parse(argv, argp, positiveUsArg, "--shift-pts-primary-period-us"))
        options->shiftPtsPrimaryPeriod = *v;
    else if (const auto v = parse(argv, argp, positiveUsArg, "--shift-pts-secondary-period-us"))
        options->shiftPtsSecondaryPeriod = *v;
    else if (const auto v = parse(argv, argp, nonNegativeIntArg, "--max-file-size-megabytes"))
        options->maxFileSizeMegabytes = *v;
    else if (const auto v = parse(argv, argp, logLevelArg, "--log-level", "-L"))
        options->logLevel = *v;
    else if (arg.startsWith("-"))
        throw InvalidArgs("Unknown arg " + enquoteAndEscape(arg) + ".");
    else
        options->cameraSets.push_back(parseCameraSet(arg));
}

/** @throws InvalidArgs */
static void validateCameraSet(const CliOptions::CameraSet& cameraSet, const CliOptions& options)
{
    if (options.cameraForFile && cameraSet.count != 1)
        throw InvalidArgs("With '--camera-for-file', parameter 'count=' must be omitted or 1.");

    if (options.cameraForFile
        && !cameraSet.secondaryFileNames.empty()
        && cameraSet.secondaryFileNames.size() != cameraSet.primaryFileNames.size())
    {
        throw InvalidArgs(lm(
            "Secondary files must be omitted or their count %1 must equal primary files count %2."
            ).args(cameraSet.primaryFileNames.size(), cameraSet.secondaryFileNames.size()));
    }

    if (options.unloopPts && !options.cameraForFile
        && (cameraSet.primaryFileNames.size() > 1 || cameraSet.secondaryFileNames.size() > 1))
    {
        throw InvalidArgs(
            "PTS unlooping is not supported when some Camera is assigned multiple files.");
    }
}

/** @throws InvalidArgs */
static void validateOptions(const CliOptions& options)
{
    if (options.shiftPts.has_value()
        + options.shiftPtsFromNow.has_value()
        + (options.shiftPtsPrimaryPeriod || options.shiftPtsSecondaryPeriod) > 1)
    {
        throw InvalidArgs("More than one PTS shifting option is specified.");
    }

    if (options.unloopPts && !options.includePts)
        throw InvalidArgs("Option '--unloop-pts' requires '--pts'.");

    if (!options.unloopPts && options.shiftPtsFromNow)
        throw InvalidArgs("Option '--shift-pts-from-now-us' requires '--unloop-pts'.");

    if (!options.unloopPts && options.shiftPtsPrimaryPeriod)
        throw InvalidArgs("Option '--shift-pts-primary-period-us' requires '--unloop-pts'.");

    if (!options.unloopPts && options.shiftPtsSecondaryPeriod)
        throw InvalidArgs("Option '--shift-pts-secondary-period-us' requires '--unloop-pts'.");

    for (const auto& cameraSet: options.cameraSets)
        validateCameraSet(cameraSet, options);
}

} // namespace

bool parseCliOptions(int argc, const char* const argv[], CliOptions* options)
{
    if (!NX_ASSERT(options))
        return false;

    if (argc == 1) //< No args specified.
        options->showHelp = true;

    try
    {
        std::optional<int> fps;
        for (int i = 1; i < argc; ++i)
            parseOption(options, argv, &i, &fps);

        if (options->showHelp)
        {
            showHelp(argv[0]);
            exit(0);
        }

        if (fps)
        {
            if (options->fpsPrimary || options->fpsSecondary)
            {
                throw InvalidArgs("Arg '--fps' must not be specified together with "
                    "'--fps-primary' or '--fps-secondary'.");
            }
            options->fpsPrimary = fps;
            options->fpsSecondary = fps;
        }

        if (ini().printOptions)
        {
            std::cerr << lm("Options parsed from command-line args:\n%1\n\n").args(
                optionsToJsonString(*options)).toStdString();
        }

        validateOptions(*options);

        return true;
    }
    catch (const InvalidArgs& e)
    {
        std::cerr << lm("ERROR: %1\n").args(e.what()).toStdString();
        return false;
    }
}

} // namespace nx::vms::testcamera
