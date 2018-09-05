-- Replace event type GUIDs with hierarchical strings in vms_businessrule::event_condition.

-- Metadata plugin "nx.hanwha".
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{5DFC6D1B-3B92-4E55-A627-0DB2A2422DB3}', 'nx.hanwha.FaceDetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{95F33B59-0CA8-4CB2-9248-1F5E616483F7}', 'nx.hanwha.FogDetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{67EDDD6A-A31F-41C4-91B6-B6298FE257A1}', 'nx.hanwha.VideoAnalytics.Passing');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{6F602403-CCA4-4F85-8777-F340E8239A23}', 'nx.hanwha.VideoAnalytics.Entering');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{2C218743-604C-475D-AED9-A848F40F13F9}', 'nx.hanwha.VideoAnalytics.Exiting');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{A452E49A-D1D8-44EA-84DA-F581414E8153}', 'nx.hanwha.VideoAnalytics.AppearDisappear,VideoAnalytics.Appearing');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{42D4994A-A267-4C39-98AA-CBC4D41CA42B}', 'nx.hanwha.VideoAnalytics.Intrusion');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{CABB4644-F6C2-4F09-8EDF-E0FAFCC4F833}', 'nx.hanwha.AudioDetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{657B39C2-36C1-4644-9357-58593658D567}', 'nx.hanwha.Tampering');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{216668D9-EA9B-483A-A43E-9414ED760A01}', 'nx.hanwha.DefocusDetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{1BAB8A57-5F19-4E3A-B73B-3641058D46B8}', 'nx.hanwha.AlarmInput.,NetworkAlarmInput');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{F1F723BB-20C8-453A-AFCE-86F318CBF097}', 'nx.hanwha.MotionDetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{79F03CE5-AFD1-4942-905C-46A981942422}', 'nx.hanwha.AudioAnalytics.Scream');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{02567E19-33AC-440B-8647-16D95EF6B295}', 'nx.hanwha.AudioAnalytics.Gunshot');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{BD275A24-F758-4FD5-9239-FBE87B1331CE}', 'nx.hanwha.AudioAnalytics.Explosion');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{BA9DFE3C-55E6-4298-BC8E-CAA947CB9440}', 'nx.hanwha.AudioAnalytics.GlassBreak');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{1B541CB7-E377-4818-8801-323281530E7B}', 'nx.hanwha.VideoAnalytics.Loitering');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{ED282564-4C25-45B1-92C3-6797D51769FE}', 'nx.hanwha.Queue.1.Level.High');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{8950220F-717A-4E75-999E-966E7B204089}', 'nx.hanwha.Queue.1.Level.Medium');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{F66BE53F-54AC-4BE0-A44A-9BA3D7D4826A}', 'nx.hanwha.Queue.2.Level.High');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{E9D8945F-45BD-43B7-B443-319AF4A8C747}', 'nx.hanwha.Queue.2.Level.Medium');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{495E9A52-66F9-41A1-A994-8F9D0D16BD18}', 'nx.hanwha.Queue.3.Level.High');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{6F44B0FC-ED45-43E8-BC8B-F361D3F2C361}', 'nx.hanwha.Queue.3.Level.Medium');

-- Metadata plugin "nx.axis".
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'f83daede-7fae-6a51-2e90-69017dadfd62', 'nx.axis.tns1:VideoSource/tnsaxis:Tampering');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'b37730fe-3e2d-9eb7-bee0-7732877ec61f', 'nx.axis.tns1:VideoSource/GlobalSceneChange/ImagingService');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '7ef7f596-3831-201a-6093-da4ae8f70e08', 'nx.axis.tns1:VideoSource/tnsaxis:DayNightVision');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '0d50994b-0126-a437-3b7f-5bff70d14424', 'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/timer');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'ed203a0c-2eae-9ce5-1022-87e993224816', 'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/linetouched');

-- Metadata plugin "nx.dw_mtt".
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'a5b9cc7c-6da5-4fe6-a2eb-19dd353fd724', 'nx.dw_mtt.MOTION');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '6a34d742-9f83-47ba-93e2-811718fa49ac', 'nx.dw_mtt.SENSOR');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '52764a47-bd7d-4447-bb4b-aaf0b0e7518f', 'nx.dw_mtt.PEA');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '48b12142-f42d-4b13-9811-9e1329568dac', 'nx.dw_mtt.PEA');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'f01f9480-e3c2-4644-8d0c-092e7aa7898d', 'nx.dw_mtt.OSC');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '7395066d-d870-4323-87aa-ca7ba2990d37', 'nx.dw_mtt.AVD');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '50108bc1-ed4d-4bde-a2d4-735614368026', 'nx.dw_mtt.AVD');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '787550c4-35bb-4490-baed-0800e03c61a6', 'nx.dw_mtt.AVD');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '2e149769-64d0-40ee-a79a-5d02962f0fe5', 'nx.dw_mtt.CPC');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '32e32ac6-e3ba-437b-a3d8-8fa9ae5b1830', 'nx.dw_mtt.IPD');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'ba0cde26-a5c9-4101-ae59-7cda70a2fa76', 'nx.dw_mtt.CDD');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '12c841a0-33a4-429e-84de-810a43738d35', 'nx.dw_mtt.VFD');

-- Metadata plugin "nx.hikvision".
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{98FF85DC-F4CA-4254-BEA5-4204AD9E82BD}', 'nx.hikvision.loitering');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{4168BB12-3005-4855-A5A2-A5753C0894A0}', 'nx.hikvision.fielddetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{792319D0-EE91-41B9-A160-C21F4BB6689B}', 'nx.hikvision.group');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{183A9FE5-BC24-455B-A799-FA57ADBA9BA1}', 'nx.hikvision.videoloss');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{90630A08-05ED-4EF0-BAC4-245B9E21FC02}', 'nx.hikvision.rapidMove');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{958C6843-93FC-49BB-9C58-05A7D5ABF19C}', 'nx.hikvision.parking');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{97C0F367-854C-4B49-8CD5-36C38344D989}', 'nx.hikvision.Face');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{BA6A2683-D0DF-4103-A7DA-832F9BB635F9}', 'nx.hikvision.LineDetect,linedetection');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{5574DD9E-AE30-4B84-9F20-A4DA1A843F48}', 'nx.hikvision.RegionEntrance');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{99EE7514-FB88-481F-8445-FD86BD47CB22}', 'nx.hikvision.RegionExiting');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{278BBB2E-F3FA-4520-8A35-486E5AF9569D}', 'nx.hikvision.AudioException');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{9D7DF4C5-A0AD-43DB-ABF9-57E9A44B6BD1}', 'nx.hikvision.Tamper,shelteralarm');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{E1AEAB1C-3D77-41AA-9D59-0EBE187B73A2}', 'nx.hikvision.Defocus');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{9DFC725A-723B-4666-8A73-A93FFAC6B001}', 'nx.hikvision.Motion,VMD');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{BE93937E-A24F-4CEE-9C51-B588DB672D4E}', 'nx.hikvision.UnattendedBaggage');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{6E62AA61-6308-467E-AF0D-C5DC34A86C42}', 'nx.hikvision.AttendedBaggage');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{6B8CF0F7-E6F0-4DAA-AD8E-7D43D6B174BF}', 'nx.hikvision.SceneChange');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{7DDF49F1-2B50-4FF9-9FB7-3898EAAB0FE2}', 'nx.hikvision.AttendedBaggage');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{349DA8B8-6F8A-4F8F-8CD9-3D34E1DC5651}', 'nx.hikvision.BlackList');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{F61119D1-BA55-4731-A541-232CB611DB1E}', 'nx.hikvision.WhiteList');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{71004AC7-D190-444E-888A-EA61997B6CD2}', 'nx.hikvision.otherlist');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '{4DAE9597-7A81-4A48-AEDC-37D5B184DB11}', 'nx.hikvision.Vehicle');

-- Metadata plugin "nx.ssc".
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '1558bdc5-3440-454e-93ed-0b34a05fedef', 'nx.ssc.camera specify command');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'e7ec745c-2ea3-43b6-9ea7-8108f47563f5', 'nx.ssc.reset command');

-- Metadata plugin "nx.vca".
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'D16B2CFB-4DCA-4081-ACB9-F4B454C6873F', 'nx.vca.md');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '93F513DE-41DD-4B4F-8623-E93D88B35174', 'nx.vca.vca');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '0D5F107B-FC5E-47C2-917C-BE54795BF9D0', 'nx.vca.fd');
