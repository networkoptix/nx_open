-- Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

use framework "Foundation"
use scripting additions

on run
    debugLog("Run: called")
    set hasRunOption to false

    -- Compiled to application script does not receive arguments as 'argv' parameter.
    -- Thus we have to extract them from process information.
    set arguments to (current application's NSProcessInfo's processInfo's arguments) as list
     -- Pass all arguments after '--run' to the client binary.
    set shellArguments to ""

    try
        repeat with arg in arguments
            if hasRunOption then
                set shellArguments to shellArguments & " " & getQuoted(arg as string)
            else if (arg as string) is equal to "--run" then
                -- '--run' option shows that we have to just run current client binary.
                set hasRunOption to true
            end if
        end repeat
    end try

    if hasRunOption then
        debugLog("Run: 'run' option is specified")
        set targetExecutable to "@client.binary.name@"
    else
        debugLog("Run: 'run' option is not specified")
        set targetExecutable to "applauncher"
    end if

    debugLog("Run: target executable is:" & targetExecutable)
    launchClient(shellArguments, targetExecutable)
end run

on open location targetUrl
    debugLog("Open: called")
    launchClient(getQuoted(targetUrl), "applauncher")
end open location

on open fileNames
    debugLog("Open files: called")
    set posixFileNames to ""
    repeat with macFileName in fileNames
        set posixFileNames to posixFileNames & " " & getQuoted(POSIX path of macFileName)
    end repeat

    launchClient(posixFileNames, "applauncher")
end open

on launchClient(arguments, targetExecutable)
    set basePath to (POSIX path of (path to me as text)) & "Contents/MacOS/"
    set command to getQuoted(basePath & targetExecutable) & " " & arguments & " > /dev/null 2>&1 &"
    debugLog("launchClient: command is: " & command as text)
    do shell script command
end launchClient

on getQuoted(value)
    return quoted form of value
end getQuoted

-- Logs specified message to the home directory and show alert with specified text
-- for debug purposes.
on debugLog(logText)
    return -- Disables logging/alerts in release by default
    set logFileName to (POSIX path of (path to home folder as text)) & "global_launcher.log"
    do shell script "echo " & getQuoted(logText) & ">>" & getQuoted(logFileName)
    display alert logText
end debugLog
