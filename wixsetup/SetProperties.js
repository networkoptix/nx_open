function LogInfo(msg)
{
    var rec = Session.Installer.CreateRecord(1);
    rec.StringData(0) = msg;
    Session.Message(0x04000000, rec);
}

function SetProperties()
{
    var WshShell = new ActiveXObject("WScript.Shell");
    var strXmlConfigFile = WshShell.ExpandEnvironmentStrings(Session.Property("XMLSETTINGS"))
    var objDoc = new ActiveXObject("msxml2.DOMDocument.3.0");
    objDoc.load(strXmlConfigFile);
    var mediaRoot = objDoc.selectSingleNode("/settings/config/mediaRoot").text;
    LogInfo("Old MEDIAFOLDER is " + mediaRoot);
    Session.Property("MEDIAFOLDER") = mediaRoot;
}