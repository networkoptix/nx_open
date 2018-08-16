on run
	launchClient("", "--- RUN:")
end run

on open location targetUrl
	launchClient(getQuoted(targetUrl), "--- OPEN LOCATION:")
end open location

on open fileNames
	set posixFileNames to ""
	repeat with macFileName in fileNames
		set posixFileNames to posixFileNames & " " & getQuoted(POSIX path of macFileName)
	end repeat
	
	launchClient(posixFileNames, "--- OPEN FILES:")
end open

on launchClient(arguments, debugLogMark)
	set basePath to (POSIX path of (path to me as text)) & "Contents/MacOS/"
	set command to getQuoted(basePath & "applauncher") & " " & arguments & " > /dev/null 2>&1 &"
	debugLog(debugLogMark & " " & command)
	do shell script command
end launchClient

on getQuoted(value)
	return quoted form of value
end getQuoted

on debugLog(logText)
        set disableDebugLog to true
	set logFileName to (POSIX path of (path to home folder as text)) & "global_launcher.log"
	if disableDebugLog then
		return
	end if
	do shell script "echo " & getQuoted(logText) & ">>" & getQuoted(logFileName)
end debugLog


