#define CompanyName "${company.name}"
#define ProductName "${product.name}"
#define Box "${box}"
#define Arch "${arch}"
#define BuildConfiguration "${build.configuration}"
#define AppVersion "${release.version}.${buildNumber}"
#define ServerUpgradeCode "${customization.upgradeCode}"
#define ClientUpgradeCode "${customization.clientUpgradeCode}"
#define NamespaceMinor "${namespace.minor}"
#define NamespaceAdditional "${namespace.additional}"

#if ${beta} = true
#define BetaSuffix "-beta"
#else
#define BetaSuffix
#endif

[Setup]
AppName={#CompanyName} {#ProductName} 
AppVersion={#AppVersion}
DefaultDirName={pf}\{#CompanyName}\{#ProductName} 
DefaultGroupName={#CompanyName}
Uninstallable=no
CreateAppDir=no
ArchitecturesAllowed={#Arch}

[Types]
Name: "full"; Description: "Launch Both Server and Client Installers"; Check: FullCheck
Name: "client"; Description: "Launch Client Installer"; Check: ClientCheck
Name: "server"; Description: "Launch Server Installer"; Check: ServerCheck

[Components]
Name: "server"; Description: "Server"; Types: full server; Check: ServerCheck
Name: "client"; Description: "Client"; Types: full client; Check: ClientCheck

[Files]
Source: "..\..\wixsetup-server-only\{#Arch}\bin\{#NamespaceMinor}-{#NamespaceAdditional}-server-{#AppVersion}-{#Box}-{#Arch}-{#BuildConfiguration}{#BetaSuffix}.msi"; DestDir: "{tmp}"; Components: server; Check: ServerCheck
Source: "..\..\wixsetup-client-only\{#Arch}\bin\{#NamespaceMinor}-{#NamespaceAdditional}-client-{#AppVersion}-{#Box}-{#Arch}-{#BuildConfiguration}{#BetaSuffix}.msi"; DestDir: "{tmp}"; Components: client; Check: ClientCheck

[Run]
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\{#NamespaceMinor}-{#NamespaceAdditional}-client-{#AppVersion}-{#Box}-{#Arch}-{#BuildConfiguration}{#BetaSuffix}.msi LICENSE_ALREADY_ACCEPTED=yes"""; Components: server; Check: ServerCheck
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\{#NamespaceMinor}-{#NamespaceAdditional}-client-{#AppVersion}-{#Box}-{#Arch}-{#BuildConfiguration}{#BetaSuffix}.msi LICENSE_ALREADY_ACCEPTED=yes"""; Components: client; Check: ClientCheck

[Code]
var
  InstallClient: Boolean;
  InstallServer: Boolean;

procedure OnTypeChange(Sender: TObject);
begin
  // set the item index in hidden TypesCombo
  WizardForm.TypesCombo.ItemIndex := TNewRadioButton(Sender).Tag;
  // notify TypesCombo about the selection change
  WizardForm.TypesCombo.OnChange(nil);
end;

procedure InitializeWizard;
var
    I: Integer;
    RadioButton: TNewRadioButton;
begin
    for I := 0 to WizardForm.TypesCombo.Items.Count - 1 do
    begin
        RadioButton := TNewRadioButton.Create(WizardForm);
        RadioButton.Parent := WizardForm.SelectComponentsPage;
        RadioButton.Left := WizardForm.TypesCombo.Left;
        RadioButton.Top := WizardForm.TypesCombo.Top + I * RadioButton.Height;
        RadioButton.Width := WizardForm.TypesCombo.Width;
        RadioButton.Checked := I = 0;
        RadioButton.Caption := WizardForm.TypesCombo.Items[I];
        RadioButton.Tag := I;
        RadioButton.TabOrder := I;     
        RadioButton.OnClick := @OnTypeChange;
    end;

    WizardForm.TypesCombo.Visible := False;

    I := WizardForm.ComponentsList.Top - (RadioButton.Top + RadioButton.Height + 8);
    WizardForm.ComponentsList.Top := RadioButton.Top + RadioButton.Height + 8;
    WizardForm.ComponentsList.Height := WizardForm.ComponentsList.Height + I;
end;

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

function GetInstalledProductVersion(upgradeCode: String): String;
var
    installer: Variant;
    relatedProducts: Variant;
    productCode: String;
    i: Integer;
begin
    Result := '';
    installer := CreateOLEObject('WindowsInstaller.Installer');
    relatedProducts := installer.RelatedProducts(upgradeCode);

    if relatedProducts.Count > 0 then
        for i := 0 to relatedProducts.Count - 1 do
        begin
            productCode := relatedProducts.Item(i);
            Result := installer.ProductInfo(productCode, 'VersionString');
        end;
end;

function InitializeSetup(): Boolean;
var
    version: String;
    previousServerVersion: String;    
    previousClientVersion: String;    
begin
    version := ExpandConstant('{#AppVersion}');
    previousServerVersion := GetInstalledProductVersion('{#ServerUpgradeCode}');
    previousClientVersion := GetInstalledProductVersion('{#ClientUpgradeCode}');

    InstallServer := (previousServerVersion <= version);
    InstallClient := (previousClientVersion <= version);

    if (NOT InstallServer AND NOT InstallClient) then
    begin
        MsgBox('Your have newer software installed. Exiting.', mbInformation, MB_OK);
        Result := False;
        exit;
    end;

    Result := True;
end;

