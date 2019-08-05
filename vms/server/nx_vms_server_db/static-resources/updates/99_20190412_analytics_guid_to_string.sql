-- Fix Analytics Event type ids in vms_businessrule::event_condition. Delete old manifests.
--
-- Deletes Analytics manifests from the database (stored as properties). The new manifests will be
-- stored by the Server when the plugins load after the upgrade. If a plugin is missing after the
-- upgrade, it has been decided to abandon proper Event type displaying in the GUI.
--
-- Fixes the consequences of the bugs in the previous update script for 3.2 -> 4.0 that replaced 
-- Analytics Event type GUIDs with hierarchical strings. Those bugs have been fixed in that update
-- script, but only after Beta 4.0.0 has been released, thus, those users who upgraded to that Beta
-- require this update script to fix their databases. This script works correctly after applying
-- any version (buggy or fixed) of the previous Analytics Event type update script.

---------------------------------------------------------------------------------------------------
-- Delete Analytics manifests

DELETE FROM vms_kvpair WHERE name = "analyticsDrivers";

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.stub"

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{b14a8d7b-8009-4d38-a60d-04139345432e}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{687611a2-fd30-94e7-7f4c-8705642b0bcc}"');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.ssc"

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{a798a4a7-a1b6-48c3-b3d0-c9b04f1e84a4}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{dd599abd-f413-5603-f4d5-62275ca394f1}"');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.vca"

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{e177ffa9-9db9-f975-e4c2-760ce7b279da}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{82213e2a-b276-e097-cc28-187ab0b0a229}"');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.dw_mtt"

-- The bugs in the previous update script: different GUIDs were replaced with identical strings:
-- 
-- Bug #1: GUIDs changed to the same 'nx.dw_mtt.PEA':
--     {52764a47-bd7d-4447-bb4b-aaf0b0e7518f}, should be 'nx.dw_mtt.PEA.perimeterAlarm'
--     {48b12142-f42d-4b13-9811-9e1329568dac}, should be 'nx.dw_mtt.PEA.tripwireAlarm'
-- 
-- Bug #2: GUIDs changed to the same 'nx.dw_mtt.AVD':
--     {7395066d-d870-4323-87aa-ca7ba2990d37}, should be 'nx.dw_mtt.AVD.sceneChange'
--     {50108bc1-ed4d-4bde-a2d4-735614368026}, should be 'nx.dw_mtt.AVD.clarityAbnormal'
--     {787550c4-35bb-4490-baed-0800e03c61a6}, should be 'nx.dw_mtt.AVD.colorAbnormal'
--
-- This update fixes that, choosing one of the particular new ids for each case - unfortunately,
-- there is no way to distinguish which Event was which.
--
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.dw_mtt.PEA', 'nx.dw_mtt.PEA.perimeterAlarm');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.dw_mtt.AVD', 'nx.dw_mtt.AVD.sceneChange');

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{3d8d6c8e-d6d5-4947-9989-38089df02a1f}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{e9d7203c-6255-a4e9-c91a-d8a439523f4e}"');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.hanwha"

-- Fix incorrect names for certain Event type ids introduced by the previous buggy update script.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.hanwha.AlarmInput.,NetworkAlarmInput', 'nx.hanwha.AlarmInput');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.hanwha.VideoAnalytics.AppearDisappear,VideoAnalytics.Appearing', 'nx.hanwha.VideoAnalytics.AppearDisappear');

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{6541f47d-99b8-4049-bf55-697bd6dd1bbd}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{d018384f-8f08-6a40-70a8-1405ba18b455}"');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.hikvision"

-- Fix incorrect names for certain Event type ids introduced by the previous buggy update script.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.hikvision.LineDetect,linedetection', 'nx.hikvision.LineDetect');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.hikvision.Tamper,shelteralarm', 'nx.hikvision.Tamper');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.hikvision.Motion,VMD', 'nx.hikvision.Motion');

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{55c9d815-d25e-4638-98e5-40ddbafeb98e}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{8c2db6a0-5422-5de4-6477-f4b77a3c474c}"');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.axis"

-- It has been decided to preserve the GUID numeric value as part of the new string id, because
-- Axis Analytics Plugin does not have hard-coded Event types but rather generates them on-the-fly,
-- and the set of Event types is not fixed and depends on which analytics apps are installed into
-- the camera from the Axis app store.
--
-- Thus, revert the replacements done by the previous buggy update script for certain Event types,
-- and introduce a prefix to GUIDs to conform with the new Event type id syntax agreement.
--
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.axis.tns1:VideoSource/tnsaxis:Tampering', '{f83daede-7fae-6a51-2e90-69017dadfd62}');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.axis.tns1:VideoSource/GlobalSceneChange/ImagingService', '{b37730fe-3e2d-9eb7-bee0-7732877ec61f}');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.axis.tns1:VideoSource/tnsaxis:DayNightVision', '{7ef7f596-3831-201a-6093-da4ae8f70e08}');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/timer', '{0d50994b-0126-a437-3b7f-5bff70d14424}');
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/linetouched', '{ed203a0c-2eae-9ce5-1022-87e993224816}');

-- Replace Plugin GUID in `instigators` field with Engine GUID in `analyticsEngineId` field.
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    ',"instigators":["{190ed57a-b435-a80b-1f7b-9f31c52ad63e}"]}',
    '},' || x'0a' || '"analyticsEngineId":"{eb46f795-9188-e608-6bd3-84b27eaaee3c}"');

-- Add prefix `nx.axis.` to Event Type ids (GUIDs).
--
-- ATTENTION: This should be the last replacement in this upgrade script: all values of inputPortId
-- in vms_businessrule::event_condition that start with `{` can only be Axis Event Type ids.
--
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '"inputPortId":"{', '"inputPortId":"nx.axis.{') WHERE event_type = 13;
