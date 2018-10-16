UPDATE vms_kvpair
SET name="userPreferredPtzPresetType"
WHERE name="disableNativePtzPresets";

UPDATE vms_kvpair
SET value="system"
WHERE name="userPreferredPtzPresetType" AND value IS NOT NULL AND value != "";

