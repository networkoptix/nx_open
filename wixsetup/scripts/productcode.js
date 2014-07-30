// Get ProductCode from existing .msi file

var msiOpenDatabaseModeTransact = 1;
var filespec = WScript.Arguments(0);
var installer = new ActiveXObject("WindowsInstaller.Installer");
var database = installer.OpenDatabase(filespec, msiOpenDatabaseModeTransact);
var sql = "SELECT Value FROM Property WHERE Property = 'ProductCode'";
var view = database.OpenView(sql);
view.Execute();

var record = view.Fetch();
WScript.Echo(record.StringData(1));
view.Close();

// sql = "DELETE FROM `Property` WHERE `Property`='ALLUSERS'";
//view = database.OpenView(sql);
//view.Execute();
//view.Close();
// database.Commit();
