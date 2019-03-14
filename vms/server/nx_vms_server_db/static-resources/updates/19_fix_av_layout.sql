delete from vms_resourcetype_parents where from_resourcetype_id in(
 select id from vms_resourcetype where name in ( 'ArecontVision_H264', 'ArecontVision_DN', 'ArecontVision_AI'));
