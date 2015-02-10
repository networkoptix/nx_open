DELETE FROM vms_cameraserveritem
WHERE exists (SELECT 1 FROM vms_cameraserveritem csi2 
				 WHERE csi2.physical_id = vms_cameraserveritem.physical_id 
                   AND csi2.timestamp = vms_cameraserveritem.timestamp 
				   AND csi2.id > vms_cameraserveritem.id);

CREATE UNIQUE INDEX idx_camera_history  ON vms_cameraserveritem(physical_id, timestamp);
