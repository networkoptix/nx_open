on run
	launchClient("", "--- RUN:")
end run

on open location targetUrl
	launchClient("--" & targetUrl, "--- OPEN LOCATION:")
end open location

on open fileNames
	set posixFileNames to ""
	repeat with macFileName in fileNames
		-- We use double quotation to preserve arguments with spaces in applauncher bash script
		set posixFileNames to posixFileNames & " " & (quoted form of ("\"" & (POSIX path of macFileName) & "\""))
	end repeat
	
	launchClient(posixFileNames, "--- OPEN FILES:")
end open

on launchClient(arguments, debugLogMark)
	set basePath to (POSIX path of (path to me as text)) & "Contents/MacOS/"
	set command to quoted form of (basePath & "applauncher") & " " & arguments & " > /dev/null 2>&1 &"
	debugLog(debugLogMark & " " & command)
	do shell script command
end launchClient

on debugLog(logText)
	set disableDebugLog to true
	set logFileName to (POSIX path of (path to home folder as text)) & "global_launcher.log"
	if disableDebugLog then
		return
	end if
	do shell script "echo " & (quoted form of logText) & ">>" & (quoted form of logFileName)
end debugLog