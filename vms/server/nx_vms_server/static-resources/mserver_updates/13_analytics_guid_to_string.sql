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
        CASE
            WHEN event_subtype IS NULL THEN
                NULL
            ELSE
                lower('{' ||
                    substr(hex(event_subtype), 1, 8) || '-' ||
                    substr(hex(event_subtype), 9, 4) || '-' ||
                    substr(hex(event_subtype), 13, 4) || '-' ||
                    substr(hex(event_subtype), 17, 4) || '-' ||
                    substr(hex(event_subtype), 21) ||
                    '}')
        END
    FROM "runtime_actions_tmp";
    
-- Replace event type GUIDs with hierarchical strings in runtime_actions::event_subtype.

-- Metadata plugin "nx.stub".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{7e94ce15-3b69-4719-8dfd-ac1b76e5d8f4}', 'nx.stub.lineCrossing');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{b0e64044-ffa3-4b7f-807a-060c1fe5a04c}', 'nx.stub.objectInTheArea');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{153dd879-1cd2-46b7-add6-7c6b48eac1fc}', 'nx.stub.car');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{c23def4d-04f7-4b4c-994e-0c0e6e8b12cb}', 'nx.stub.humanFace');

-- Metadata plugin "nx.hanwha".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{e068d6ff-a3ac-43aa-b2d6-1804440af506}', 'nx.hanwha.ShockDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{5dfc6d1b-3b92-4e55-a627-0db2a2422db3}', 'nx.hanwha.FaceDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{95f33b59-0ca8-4cb2-9248-1f5e616483f7}', 'nx.hanwha.FogDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{67eddd6a-a31f-41c4-91b6-b6298fe257a1}', 'nx.hanwha.VideoAnalytics.Passing');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{6f602403-cca4-4f85-8777-f340e8239a23}', 'nx.hanwha.VideoAnalytics.Entering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{2c218743-604c-475d-aed9-a848f40f13f9}', 'nx.hanwha.VideoAnalytics.Exiting');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{a452e49a-d1d8-44ea-84da-f581414e8153}', 'nx.hanwha.VideoAnalytics.AppearDisappear,VideoAnalytics.Appearing');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{42d4994a-a267-4c39-98aa-cbc4d41ca42b}', 'nx.hanwha.VideoAnalytics.Intrusion');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{cabb4644-f6c2-4f09-8edf-e0fafcc4f833}', 'nx.hanwha.AudioDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{657b39c2-36c1-4644-9357-58593658d567}', 'nx.hanwha.Tampering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{216668d9-ea9b-483a-a43e-9414ed760a01}', 'nx.hanwha.DefocusDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{1bab8a57-5f19-4e3a-b73b-3641058d46b8}', 'nx.hanwha.AlarmInput.,NetworkAlarmInput');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{f1f723bb-20c8-453a-afce-86f318cbf097}', 'nx.hanwha.MotionDetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{79f03ce5-afd1-4942-905c-46a981942422}', 'nx.hanwha.AudioAnalytics.Scream');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{02567e19-33ac-440b-8647-16d95ef6b295}', 'nx.hanwha.AudioAnalytics.Gunshot');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{bd275a24-f758-4fd5-9239-fbe87b1331ce}', 'nx.hanwha.AudioAnalytics.Explosion');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{ba9dfe3c-55e6-4298-bc8e-caa947cb9440}', 'nx.hanwha.AudioAnalytics.GlassBreak');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{1b541cb7-e377-4818-8801-323281530e7b}', 'nx.hanwha.VideoAnalytics.Loitering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{ed282564-4c25-45b1-92c3-6797d51769fe}', 'nx.hanwha.Queue.1.Level.High');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{8950220f-717a-4e75-999e-966e7b204089}', 'nx.hanwha.Queue.1.Level.Medium');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{f66be53f-54ac-4be0-a44a-9ba3d7d4826a}', 'nx.hanwha.Queue.2.Level.High');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{e9d8945f-45bd-43b7-b443-319af4a8c747}', 'nx.hanwha.Queue.2.Level.Medium');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{495e9a52-66f9-41a1-a994-8f9d0d16bd18}', 'nx.hanwha.Queue.3.Level.High');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{6f44b0fc-ed45-43e8-bc8b-f361d3f2c361}', 'nx.hanwha.Queue.3.Level.Medium');

-- Metadata plugin "nx.axis".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{f83daede-7fae-6a51-2e90-69017dadfd62}', 'nx.axis.tns1:VideoSource/tnsaxis:Tampering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{b37730fe-3e2d-9eb7-bee0-7732877ec61f}', 'nx.axis.tns1:VideoSource/GlobalSceneChange/ImagingService');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{7ef7f596-3831-201a-6093-da4ae8f70e08}', 'nx.axis.tns1:VideoSource/tnsaxis:DayNightVision');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{0d50994b-0126-a437-3b7f-5bff70d14424}', 'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/timer');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{ed203a0c-2eae-9ce5-1022-87e993224816}', 'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/linetouched');

-- Metadata plugin "nx.dw_mtt".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{a5b9cc7c-6da5-4fe6-a2eb-19dd353fd724}', 'nx.dw_mtt.MOTION');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{6a34d742-9f83-47ba-93e2-811718fa49ac}', 'nx.dw_mtt.SENSOR');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{52764a47-bd7d-4447-bb4b-aaf0b0e7518f}', 'nx.dw_mtt.PEA');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{48b12142-f42d-4b13-9811-9e1329568dac}', 'nx.dw_mtt.PEA');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{f01f9480-e3c2-4644-8d0c-092e7aa7898d}', 'nx.dw_mtt.OSC');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{7395066d-d870-4323-87aa-ca7ba2990d37}', 'nx.dw_mtt.AVD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{50108bc1-ed4d-4bde-a2d4-735614368026}', 'nx.dw_mtt.AVD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{787550c4-35bb-4490-baed-0800e03c61a6}', 'nx.dw_mtt.AVD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{2e149769-64d0-40ee-a79a-5d02962f0fe5}', 'nx.dw_mtt.CPC');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{32e32ac6-e3ba-437b-a3d8-8fa9ae5b1830}', 'nx.dw_mtt.IPD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{ba0cde26-a5c9-4101-ae59-7cda70a2fa76}', 'nx.dw_mtt.CDD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{12c841a0-33a4-429e-84de-810a43738d35}', 'nx.dw_mtt.VFD');

-- Metadata plugin "nx.hikvision".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{98ff85dc-f4ca-4254-bea5-4204ad9e82bd}', 'nx.hikvision.loitering');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{4168bb12-3005-4855-a5a2-a5753c0894a0}', 'nx.hikvision.fielddetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{792319d0-ee91-41b9-a160-c21f4bb6689b}', 'nx.hikvision.group');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{183a9fe5-bc24-455b-a799-fa57adba9ba1}', 'nx.hikvision.videoloss');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{90630a08-05ed-4ef0-bac4-245b9e21fc02}', 'nx.hikvision.rapidMove');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{958c6843-93fc-49bb-9c58-05a7d5abf19c}', 'nx.hikvision.parking');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{97c0f367-854c-4b49-8cd5-36c38344d989}', 'nx.hikvision.Face');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{ba6a2683-d0df-4103-a7da-832f9bb635f9}', 'nx.hikvision.LineDetect,linedetection');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{5574dd9e-ae30-4b84-9f20-a4da1a843f48}', 'nx.hikvision.RegionEntrance');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{99ee7514-fb88-481f-8445-fd86bd47cb22}', 'nx.hikvision.RegionExiting');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{278bbb2e-f3fa-4520-8a35-486e5af9569d}', 'nx.hikvision.AudioException');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{9d7df4c5-a0ad-43db-abf9-57e9a44b6bd1}', 'nx.hikvision.Tamper,shelteralarm');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{e1aeab1c-3d77-41aa-9d59-0ebe187b73a2}', 'nx.hikvision.Defocus');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{9dfc725a-723b-4666-8a73-a93ffac6b001}', 'nx.hikvision.Motion,VMD');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{be93937e-a24f-4cee-9c51-b588db672d4e}', 'nx.hikvision.UnattendedBaggage');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{6e62aa61-6308-467e-af0d-c5dc34a86c42}', 'nx.hikvision.AttendedBaggage');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{6b8cf0f7-e6f0-4daa-ad8e-7d43d6b174bf}', 'nx.hikvision.SceneChange');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{7ddf49f1-2b50-4ff9-9fb7-3898eaab0fe2}', 'nx.hikvision.AttendedBaggage');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{349da8b8-6f8a-4f8f-8cd9-3d34e1dc5651}', 'nx.hikvision.BlackList');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{f61119d1-ba55-4731-a541-232cb611db1e}', 'nx.hikvision.WhiteList');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{71004ac7-d190-444e-888a-ea61997b6cd2}', 'nx.hikvision.otherlist');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{4dae9597-7a81-4a48-aedc-37d5b184db11}', 'nx.hikvision.Vehicle');

-- Metadata plugin "nx.ssc".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{1558bdc5-3440-454e-93ed-0b34a05fedef}', 'nx.ssc.camera specify command');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{e7ec745c-2ea3-43b6-9ea7-8108f47563f5}', 'nx.ssc.reset command');

-- Metadata plugin "nx.vca".
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{d16b2cfb-4dca-4081-acb9-f4b454c6873f}', 'nx.vca.md');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{93f513de-41dd-4b4f-8623-e93d88b35174}', 'nx.vca.vca');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    '{0d5f107b-fc5e-47c2-917c-be54795bf9d0}', 'nx.vca.fd');

DROP TABLE "runtime_actions_tmp";
