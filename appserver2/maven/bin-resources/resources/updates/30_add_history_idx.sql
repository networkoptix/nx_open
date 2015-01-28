DELETE FROM vms_cameraserveritem
WHERE exists (SELECT 1 from vms_cameraserveritem csi2 WHERE csi2.physical_id = physical_id and csi2.timestamp = timestamp and csi2.id > id);

CREATE UNIQUE INDEX idx_camera_history  ON vms_cameraserveritem(physical_id, timestamp);
