-- Replace event type GUIDs with hierarchical strings in runtime_actions::event_subtype,
-- changing the column type from blob to string.

ALTER TABLE "runtime_actions" RENAME TO "runtime_actions_tmp";

CREATE TABLE "runtime_actions" (
    timestamp INTEGER NOT NULL,
    action_type SMALLINT NOT NULL,
    action_params TEXT,
    runtime_params TEXT,
    business_rule_guid BLOB(16),
    toggle_state SMALLINT,
    aggregation_count INTEGER,
    event_type SMALLINT,
    event_resource_GUID BLOB(16),
    action_resource_guid BLOB(16),
    event_subtype TEXT
);

-- Copy old table, changing event_subtype BLOB to hex string.
INSERT INTO "runtime_actions"
    SELECT
        timestamp,
        action_type,
        action_params,
        runtime_params,
        business_rule_guid,
        toggle_state,
        aggregation_count,
        event_type,
        event_resource_GUID,
        action_resource_guid,
        hex(event_subtype)
    FROM "runtime_actions_tmp";
    
-- Replace event type GUIDs with hierarchical strings in runtime_actions::event_subtype.

-- Metadata plugin "nx.hanwha".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '5DFC6D1B3B924E55A6270DB2A2422DB3', 'nx.hanwha.FaceDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '95F33B590CA84CB292481F5E616483F7', 'nx.hanwha.FogDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '67EDDD6AA31F41C491B6B6298FE257A1', 'nx.hanwha.VideoAnalytics.Passing');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '6F602403CCA44F858777F340E8239A23', 'nx.hanwha.VideoAnalytics.Entering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '2C218743604C475DAED9A848F40F13F9', 'nx.hanwha.VideoAnalytics.Exiting');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'A452E49AD1D844EA84DAF581414E8153', 'nx.hanwha.VideoAnalytics.AppearDisappear,VideoAnalytics.Appearing');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '42D4994AA2674C3998AACBC4D41CA42B', 'nx.hanwha.VideoAnalytics.Intrusion');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'CABB4644F6C24F098EDFE0FAFCC4F833', 'nx.hanwha.AudioDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '657B39C236C14644935758593658D567', 'nx.hanwha.Tampering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '216668D9EA9B483AA43E9414ED760A01', 'nx.hanwha.DefocusDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '1BAB8A575F194E3AB73B3641058D46B8', 'nx.hanwha.AlarmInput.,NetworkAlarmInput');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'F1F723BB20C8453AAFCE86F318CBF097', 'nx.hanwha.MotionDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '79F03CE5AFD14942905C46A981942422', 'nx.hanwha.AudioAnalytics.Scream');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '02567E1933AC440B864716D95EF6B295', 'nx.hanwha.AudioAnalytics.Gunshot');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'BD275A24F7584FD59239FBE87B1331CE', 'nx.hanwha.AudioAnalytics.Explosion');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'BA9DFE3C55E64298BC8ECAA947CB9440', 'nx.hanwha.AudioAnalytics.GlassBreak');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '1B541CB7E37748188801323281530E7B', 'nx.hanwha.VideoAnalytics.Loitering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'ED2825644C2545B192C36797D51769FE', 'nx.hanwha.Queue.1.Level.High');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '8950220F717A4E75999E966E7B204089', 'nx.hanwha.Queue.1.Level.Medium');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'F66BE53F54AC4BE0A44A9BA3D7D4826A', 'nx.hanwha.Queue.2.Level.High');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'E9D8945F45BD43B7B443319AF4A8C747', 'nx.hanwha.Queue.2.Level.Medium');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '495E9A5266F941A1A9948F9D0D16BD18', 'nx.hanwha.Queue.3.Level.High');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '6F44B0FCED4543E8BC8BF361D3F2C361', 'nx.hanwha.Queue.3.Level.Medium');

-- Metadata plugin "nx.axis".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'F83DAEDE7FAE6A512E9069017DADFD62', 'nx.axis.tns1:VideoSource/tnsaxis:Tampering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'B37730FE3E2D9EB7BEE07732877EC61F', 'nx.axis.tns1:VideoSource/GlobalSceneChange/ImagingService');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '7EF7F5963831201A6093DA4AE8F70E08', 'nx.axis.tns1:VideoSource/tnsaxis:DayNightVision');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '0D50994B0126A4373B7F5BFF70D14424', 'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/timer');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'ED203A0C2EAE9CE5102287E993224816', 'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/linetouched');

-- Metadata plugin "nx.dw_mtt".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'A5B9CC7C6DA54FE6A2EB19DD353FD724', 'nx.dw_mtt.MOTION');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '6A34D7429F8347BA93E2811718FA49AC', 'nx.dw_mtt.SENSOR');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '52764A47BD7D4447BB4BAAF0B0E7518F', 'nx.dw_mtt.PEA');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '48B12142F42D4B1398119E1329568DAC', 'nx.dw_mtt.PEA');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'F01F9480E3C246448D0C092E7AA7898D', 'nx.dw_mtt.OSC');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '7395066DD870432387AACA7BA2990D37', 'nx.dw_mtt.AVD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '50108BC1ED4D4BDEA2D4735614368026', 'nx.dw_mtt.AVD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '787550C435BB4490BAED0800E03C61A6', 'nx.dw_mtt.AVD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '2E14976964D040EEA79A5D02962F0FE5', 'nx.dw_mtt.CPC');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '32E32AC6E3BA437BA3D88FA9AE5B1830', 'nx.dw_mtt.IPD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'BA0CDE26A5C94101AE597CDA70A2FA76', 'nx.dw_mtt.CDD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '12C841A033A4429E84DE810A43738D35', 'nx.dw_mtt.VFD');

-- Metadata plugin "nx.hikvision".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '98FF85DCF4CA4254BEA54204AD9E82BD', 'nx.hikvision.loitering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '4168BB1230054855A5A2A5753C0894A0', 'nx.hikvision.fielddetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '792319D0EE9141B9A160C21F4BB6689B', 'nx.hikvision.group');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '183A9FE5BC24455BA799FA57ADBA9BA1', 'nx.hikvision.videoloss');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '90630A0805ED4EF0BAC4245B9E21FC02', 'nx.hikvision.rapidMove');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '958C684393FC49BB9C5805A7D5ABF19C', 'nx.hikvision.parking');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '97C0F367854C4B498CD536C38344D989', 'nx.hikvision.Face');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'BA6A2683D0DF4103A7DA832F9BB635F9', 'nx.hikvision.LineDetect,linedetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '5574DD9EAE304B849F20A4DA1A843F48', 'nx.hikvision.RegionEntrance');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '99EE7514FB88481F8445FD86BD47CB22', 'nx.hikvision.RegionExiting');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '278BBB2EF3FA45208A35486E5AF9569D', 'nx.hikvision.AudioException');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '9D7DF4C5A0AD43DBABF957E9A44B6BD1', 'nx.hikvision.Tamper,shelteralarm');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'E1AEAB1C3D7741AA9D590EBE187B73A2', 'nx.hikvision.Defocus');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '9DFC725A723B46668A73A93FFAC6B001', 'nx.hikvision.Motion,VMD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'BE93937EA24F4CEE9C51B588DB672D4E', 'nx.hikvision.UnattendedBaggage');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '6E62AA616308467EAF0DC5DC34A86C42', 'nx.hikvision.AttendedBaggage');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '6B8CF0F7E6F04DAAAD8E7D43D6B174BF', 'nx.hikvision.SceneChange');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '7DDF49F12B504FF99FB73898EAAB0FE2', 'nx.hikvision.AttendedBaggage');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '349DA8B86F8A4F8F8CD93D34E1DC5651', 'nx.hikvision.BlackList');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'F61119D1BA554731A541232CB611DB1E', 'nx.hikvision.WhiteList');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '71004AC7D190444E888AEA61997B6CD2', 'nx.hikvision.otherlist');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '4DAE95977A814A48AEDC37D5B184DB11', 'nx.hikvision.Vehicle');

-- Metadata plugin "nx.ssc".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '1558BDC53440454E93ED0B34A05FEDEF', 'nx.ssc.camera specify command');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'E7EC745C2EA343B69EA78108F47563F5', 'nx.ssc.reset command');

-- Metadata plugin "nx.vca".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'D16B2CFB4DCA4081ACB9F4B454C6873F', 'nx.vca.md');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '93F513DE41DD4B4F8623E93D88B35174', 'nx.vca.vca');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '0D5F107BFC5E47C2917CBE54795BF9D0', 'nx.vca.fd');

DROP TABLE "runtime_actions_tmp";
