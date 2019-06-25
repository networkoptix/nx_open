-- Fix Analytics Event type ids in runtime_actions::event_subtype.
--
-- Fixes the consequences of the bugs in the previous update script for 3.2 -> 4.0 that replaced 
-- Analytics Event type GUIDs with hierarchical strings. Those bugs have been fixed in that update
-- script, but only after Beta 4.0.0 has been released, thus, those users who upgraded to that Beta
-- require this update script to fix their databases. This script works correctly after applying
-- any version (buggy or fixed) of the previous Analytics Event type update script.

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
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.dw_mtt.PEA', 'nx.dw_mtt.PEA.perimeterAlarm');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.dw_mtt.AVD', 'nx.dw_mtt.AVD.sceneChange');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.hanwha"

-- Fix incorrect names for certain Event type ids introduced by the previous buggy update script.
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.hanwha.AlarmInput.,NetworkAlarmInput', 'nx.hanwha.AlarmInput');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.hanwha.VideoAnalytics.AppearDisappear,VideoAnalytics.Appearing', 'nx.hanwha.VideoAnalytics.AppearDisappear');

---------------------------------------------------------------------------------------------------
-- Analytics plugin "nx.hikvision"

-- Fix incorrect names for certain Event type ids introduced by the previous buggy update script.
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.hikvision.LineDetect,linedetection', 'nx.hikvision.LineDetect');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.hikvision.Tamper,shelteralarm', 'nx.hikvision.Tamper');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.hikvision.Motion,VMD', 'nx.hikvision.Motion');

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
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.axis.tns1:VideoSource/tnsaxis:Tampering', '{f83daede-7fae-6a51-2e90-69017dadfd62}');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.axis.tns1:VideoSource/GlobalSceneChange/ImagingService', '{b37730fe-3e2d-9eb7-bee0-7732877ec61f}');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.axis.tns1:VideoSource/tnsaxis:DayNightVision', '{7ef7f596-3831-201a-6093-da4ae8f70e08}');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/timer', '{0d50994b-0126-a437-3b7f-5bff70d14424}');
UPDATE runtime_actions SET event_subtype = replace(event_subtype,
    'nx.axis.tns1:RuleEngine/tnsaxis:CrossLineDetection/linetouched', '{ed203a0c-2eae-9ce5-1022-87e993224816}');

-- Add prefix `nx.axis.` to Event Type ids (GUIDs).
--
-- ATTENTION: This should be the last replacement in this upgrade script: all remaining GUIDs in
-- runtime_actions::event_subtype for Analytics Events (event_type 13) are Axis Event Type ids -
-- other Event Type Ids have already been replaced with human-readable identifiers.
--
UPDATE runtime_actions SET event_subtype = 'nx.axis.' || event_subtype
    WHERE event_subtype LIKE '{________-____-____-____-____________}' AND event_type = 13;
