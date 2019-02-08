-- Replace user "admin" with role "Owner"
UPDATE vms_businessrule_action_resources
SET resource_guid = X'00000000000000000000100000000000'
WHERE resource_guid = X'99cbc715539b4bfe856f799b45b69b1e';
