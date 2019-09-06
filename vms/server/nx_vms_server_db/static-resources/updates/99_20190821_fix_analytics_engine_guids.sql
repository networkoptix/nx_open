--------------------------------------------------------------------------------------------------
---- Axis plugin
    
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '"analyticsEngineId":"{eb46f795-9188-e608-6bd3-84b27eaaee3c}"',
    '"analyticsEngineId":"{cb4a4ec3-31e4-7fe7-ba2a-e3464b407edc}"');

--------------------------------------------------------------------------------------------------
---- Hikvision plugin
    
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '"analyticsEngineId":"{8c2db6a0-5422-5de4-6477-f4b77a3c474c}"',
    '"analyticsEngineId":"{1e5613c4-b7ac-546d-6623-8c179de18114}"');

--------------------------------------------------------------------------------------------------
---- VCA plugin
    
UPDATE vms_businessrule SET event_condition = replace(event_condition,
    '"analyticsEngineId":"{82213e2a-b276-e097-cc28-187ab0b0a229}"',
    '"analyticsEngineId":"{a6f9ed2c-261f-be90-5627-5bac6d0e7110}"');
    