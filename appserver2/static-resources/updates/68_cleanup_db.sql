DELETE FROM 
  vms_resource
WHERE
  guid IS NULL 
    OR
  guid = X'0000000000000000';	

DELETE FROM 
  vms_camera_user_attributes
WHERE 
  camera_guid NOT IN (
    SELECT 
      guid
    FROM
      vms_resource
    );
    
DELETE FROM
  vms_server_user_attributes
WHERE
  server_guid NOT IN (
    SELECT
      guid
    FROM 
      vms_resource
  );
  
DELETE FROM
  vms_kvpair
WHERE
  resource_guid NOT IN (
    SELECT
      guid
    FROM 
      vms_resource
  );
  
DELETE FROM
  vms_resource_status
WHERE
  guid NOT IN (
    SELECT
      guid
    FROM
      vms_resource
  );