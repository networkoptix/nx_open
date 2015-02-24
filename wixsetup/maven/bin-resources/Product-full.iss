﻿#include "variables.iss"
#include "generated_variables.iss"

[Setup]
AppName={#CompanyName} {#ProductName} 
AppVersion={#AppVersion}
DefaultDirName={pf}\{#CompanyName}\{#ProductName} 
DefaultGroupName={#CompanyName}
Uninstallable=no
CreateAppDir=no
ArchitecturesAllowed={#Arch}
LicenseFile=License.rtf

[Languages]
#if Lang == "ru-ru"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
#elif Lang == "zh-CN"
Name: "cn"; MessagesFile: "Languages\ChineseSimplified.isl"
#elif Lang == "zh-TW"
Name: "tr_cn"; MessagesFile: "Languages\ChineseTraditional.isl"
#elif Lang == "ko-kr"
Name: "ko"; MessagesFile: "Languages\korean.isl"
#else
Name: "en"; MessagesFile: "compiler:Default.isl"
#endif

[CustomMessages]
#if Lang == "ru-ru"
ru.ClientIsAlreadyInstalled=Клиент уже установлен
ru.ServerIsAlreadyInstalled=Сервер уже установлен
ru.LaunchBoth=Запустить оба инсталлятора
ru.LaunchClient=Запустить инсталлятор клиента
ru.LaunchServer=Запустить инсталлятор сервера
ru.NewerVersionAlreadyInstalled=Установлена более поздняя версия системы. Работа завершена.
#elif Lang == "zh-CN"
cn.ClientIsAlreadyInstalled=客户端已安装
cn.ServerIsAlreadyInstalled=服务器已安装
cn.LaunchBoth=启动服务器和客户端安装程序
cn.LaunchClient=启动客户端安装程序
cn.LaunchServer=启动服务器安装程序
cn.NewerVersionAlreadyInstalled=已安装更新的软件，退出。
#elif Lang == "zh-TW"
tr_cn.ClientIsAlreadyInstalled=客戶端已安裝
tr_cn.ServerIsAlreadyInstalled=伺服器已安裝
tr_cn.LaunchBoth=啟動伺服器與客戶端安裝程序
tr_cn.LaunchClient=啟動客戶端安裝程序
tr_cn.LaunchServer=啟動伺服器安裝程序
tr_cn.NewerVersionAlreadyInstalled=您已安裝較新的版本, 程序即將結束.
#elif Lang == "ko-kr"
ko.ClientIsAlreadyInstalled=클라이언트가 이미 설치되었습니다
ko.ServerIsAlreadyInstalled=서버가 이미 설치되었습니다
ko.LaunchBoth=서버와 클리아언트 설치 시작
ko.LaunchClient=클라이언트 설치 시작
ko.LaunchServer=서버 설치 시작
ko.NewerVersionAlreadyInstalled=새로운 소프트웨어가 설치 되었습니다. 종료.
#else
en.ClientIsAlreadyInstalled=Client is already installed
en.ServerIsAlreadyInstalled=Server is already installed
en.LaunchBoth=Launch Both Server and Client Installers
en.LaunchClient=Launch Client Installer
en.LaunchServer=Launch Server Installer
en.NewerVersionAlreadyInstalled=Your have newer software installed. Exiting.
#endif

[Types]
Name: "full"; Description: "{cm:LaunchBoth}"
Name: "client"; Description: "{cm:LaunchClient}"
Name: "server"; Description: "{cm:LaunchServer}"

[Components]
Name: "server"; Description: "Server"; Types: full server; Check: ServerCheck
Name: "client"; Description: "Client"; Types: full client; Check: ClientCheck

[Files]
Source: "{#ServerMsiFolder}\{#ServerMsiName}"; DestDir: "{tmp}"; Components: server; Check: ServerCheck
Source: "{#ClientMsiFolder}\{#ClientMsiName}"; DestDir: "{tmp}"; Components: client; Check: ClientCheck

[Run]
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\{#ServerMsiName}"" LICENSE_ALREADY_ACCEPTED=yes"; Components: server; Check: ServerCheck
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\{#ClientMsiName}"" LICENSE_ALREADY_ACCEPTED=yes"; Components: client; Check: ClientCheck

[Code]
var
  InstallClient: Boolean;
  InstallServer: Boolean;

function BoolToStr( Value : boolean ) : string;
begin
   if ( not Value ) then
      result := 'false'
   else
      result := 'true';
end;

function ServerCheck(): Boolean;
begin
  Log('ServerCheck ' + BoolToStr(InstallServer));
  Result := InstallServer;
end;

function ClientCheck(): Boolean;
begin
  Log('ClientCheck ' + BoolToStr(InstallClient));
  Result := InstallClient;
end;

function FullCheck(): Boolean;
begin
  Result := InstallServer AND InstallClient;
end;

procedure OnTypeChange(Sender: TObject);
begin
  // set the item index in hidden TypesCombo
  WizardForm.TypesCombo.ItemIndex := TNewRadioButton(Sender).Tag;
  // notify TypesCombo about the selection change
  WizardForm.TypesCombo.OnChange(nil);
end;

function IsTypeEnabled(atype: Integer): Boolean;
begin
    if (atype = 0) then
        Result := (InstallServer AND InstallClient)
    else if (atype = 1) then
        Result := InstallClient
    else 
        Result := InstallServer
end;

procedure InitializeWizard;
var
    I: Integer;
    textLabel: TNewStaticText;
    RadioButton: TNewRadioButton;
    CheckedIsSet: Boolean;
begin
    CheckedIsSet := False;
    for I := 0 to WizardForm.TypesCombo.Items.Count - 1 do
    begin
        RadioButton := TNewRadioButton.Create(WizardForm);
        RadioButton.Parent := WizardForm.SelectComponentsPage;
        RadioButton.Left := WizardForm.TypesCombo.Left;
        RadioButton.Top := WizardForm.TypesCombo.Top + I * RadioButton.Height;
        RadioButton.Width := WizardForm.TypesCombo.Width;
        if (NOT CheckedIsSet AND IsTypeEnabled(I)) then
        begin
            CheckedIsSet := True;
            RadioButton.Checked := True;
        end
        else
            RadioButton.Checked := False;
            
        RadioButton.Caption := WizardForm.TypesCombo.Items[I];
        RadioButton.Tag := I;
        RadioButton.TabOrder := I;     
        RadioButton.OnClick := @OnTypeChange;
        RadioButton.Enabled := IsTypeEnabled(I);
    end;

    if (NOT FullCheck()) then
    begin
        textLabel := TNewStaticText.Create(WizardForm)
        textLabel.Parent := WizardForm.SelectComponentsPage;
        textLabel.Left := WizardForm.TypesCombo.Left;
        textLabel.Top := WizardForm.TypesCombo.Top;
        textLabel.Width := WizardForm.TypesCombo.Width;
        if (NOT ServerCheck()) then
            textLabel.Caption := CustomMessage('ServerIsAlreadyInstalled');

        if (NOT ClientCheck()) then
            textLabel.Caption := CustomMessage('ClientIsAlreadyInstalled');

        I := WizardForm.ComponentsList.Top - (RadioButton.Top + RadioButton.Height + 8);
        textLabel.Top := RadioButton.Top + RadioButton.Height + 8;
        textLabel.Height := WizardForm.ComponentsList.Height + I;
    end;

    WizardForm.TypesCombo.Visible := False;

    I := WizardForm.ComponentsList.Top - (RadioButton.Top + RadioButton.Height + 8);
    WizardForm.ComponentsList.Top := RadioButton.Top + RadioButton.Height + 8;
    WizardForm.ComponentsList.Height := WizardForm.ComponentsList.Height + I;
end;

procedure Explode(var Dest: TArrayOfString; Text: String; Separator: String);
var
  i, p: Integer;
begin
  i := 0;
  repeat
    SetArrayLength(Dest, i+1);
    p := Pos(Separator,Text);
    if p > 0 then begin
      Dest[i] := Copy(Text, 1, p-1);
      Text := Copy(Text, p + Length(Separator), Length(Text));
      i := i + 1;
    end else begin
      Dest[i] := Text;
      Text := '';
    end;
  until Length(Text)=0;
end;

function VersionAsNumber(versionString: String): Integer;
var
    versionArray: TArrayOfString;
    multiplier: Integer;
    version: Integer;
    i: Integer;
begin
    Result := 0;

    if (Length(versionString) = 0) then exit;

    Explode(versionArray, versionString, '.');
    version := 0;
    multiplier := 1;

    for i := 2 downto 0 do
    begin
        version := version + StrToInt(versionArray[i]) * multiplier;
        multiplier := multiplier * 256;
    end;

    Result := version;
end;

function GetInstalledProductCode(installer: Variant; upgradeCode: String): String;
var
    relatedProducts: Variant;
begin
    Result := '';
    relatedProducts := installer.RelatedProducts(upgradeCode);

    if relatedProducts.Count > 0 then
        Result := relatedProducts.Item(0);
end;

function GetInstalledProductVersion(installer: Variant; productCode: String): String;
begin
    Result := '';

    if (productCode = '') then Exit;

    Result := installer.ProductInfo(productCode, 'VersionString');
end;

function InitializeSetup(): Boolean;
var
    installer: Variant;
    version: String;
    previousServerProductCode: String;
    previousClientProductCode: String;
    previousServerVersion: String;    
    previousClientVersion: String;    
begin
    installer := CreateOLEObject('WindowsInstaller.Installer');
    version := ExpandConstant('{#AppVersion}');

    previousServerProductCode := GetInstalledProductCode(installer, '{#ServerUpgradeCode}');
    previousClientProductCode := GetInstalledProductCode(installer, '{#ClientUpgradeCode}');

    previousServerVersion := GetInstalledProductVersion(installer, previousServerProductCode);
    previousClientVersion := GetInstalledProductVersion(installer, previousClientProductCode);

    // If not installed -> previousServerVersion and previousServerProductCode are '', same for client
    InstallServer := ((previousServerVersion <= version) AND (previousServerProductCode <> '{#ServerMsiProductCode}'));
    InstallClient := ((previousClientVersion <= version) AND (previousClientProductCode <> '{#ClientMsiProductCode}'));

    if (NOT InstallServer AND NOT InstallClient) then
    begin
        MsgBox(CustomMessage('NewerVersionAlreadyInstalled'), mbInformation, MB_OK);
        Result := False;
        exit;
    end;

    Result := True;
end;

