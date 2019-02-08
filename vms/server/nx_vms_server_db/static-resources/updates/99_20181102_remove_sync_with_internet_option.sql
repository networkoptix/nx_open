UPDATE vms_kvpair 
  SET value = '' 
  WHERE name = 'primaryTimeServer' AND exists (
    SELECT 1 FROM vms_kvpair WHERE name = 'synchronizeTimeWithInternet' and (value = '1' or value='true'));
