var msiOpenDatabaseModeTransact = 1;
var filespec = WScript.Arguments(0);
var installer = new ActiveXObject("WindowsInstaller.Installer");
var database = installer.OpenDatabase(filespec, msiOpenDatabaseModeTransact);
var sql = "UPDATE `Control` SET `Control`.`Height` = '18', `Control`.`Width` = '170'," +
          " `Control`.`Y`='243', `Control`.`X`='10' " +
          "WHERE `Control`.`Dialog_`='MyExitDialog' AND " + 
          "  `Control`.`Control`='OptionalCheckBox'";
var view = database.OpenView(sql);
view.Execute();
view.Close();

// sql = "DELETE FROM `Property` WHERE `Property`='ALLUSERS'";
//view = database.OpenView(sql);
//view.Execute();
//view.Close();
database.Commit();
