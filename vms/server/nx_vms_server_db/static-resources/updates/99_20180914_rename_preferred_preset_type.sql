DELETE FROM vms_kvpair
WHERE name="disableNativePtzPresets" 
  AND EXISTS (SELECT 1 
                FROM vms_kvpair kv2 
               WHERE kv2.resource_guid = vms_kvpair.resource_guid 
                 AND kv2.name = "userPreferredPtzPresetType");


UPDATE vms_kvpair
SET name="userPreferredPtzPresetType"
WHERE name="disableNativePtzPresets";

UPDATE vms_kvpair
SET value="system"
WHERE name="userPreferredPtzPresetType" AND value IS NOT NULL AND value != "";

