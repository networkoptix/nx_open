-- Add recording thresholds to the camera attributes.

ALTER TABLE vms_camera_user_attributes ADD record_before_motion_sec integer;
ALTER TABLE vms_camera_user_attributes ADD record_after_motion_sec integer;
