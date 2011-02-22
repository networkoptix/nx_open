Function LogInfo(msg) Dim rec
    Set rec = Session.Installer.CreateRecord(1) 
    rec.StringData(0) = msg
    LogInfo = Session.Message(&H04000000, rec)
End Function

Sub SetProperties 
    On Error Resume Next

    Set WSHShell = CreateObject("WScript.Shell")
    strXmlConfigFile = WshShell.ExpandEnvironmentStrings(Session.Property("XMLSETTINGS"))
    Set objDoc = CreateObject("msxml2.DOMDocument.3.0") 
    objDoc.Load(strXmlConfigFile) 
    mediaRoot = objDoc.selectSingleNode("/settings/config/mediaRoot").text
    LogInfo "Old MEDIAFOLDER is " & mediaRoot
    Session.Property("MEDIAFOLDER") = mediaRoot
    Set objDoc = Nothing
End Sub 

