UPDATE vms_camera_user_attributes
SET dewarping_params = replace( dewarping_params, '"viewMode": "0"', '"hStretch": 1, "viewMode": "Horizontal"' ) WHERE dewarping_params LIKE '%"viewMode": "0"%';

UPDATE vms_camera_user_attributes
SET dewarping_params = replace( dewarping_params, '"viewMode": "1"', '"hStretch": 1, "viewMode": "VerticalDown"' ) WHERE dewarping_params LIKE '%"viewMode": "1"%';

UPDATE vms_camera_user_attributes
SET dewarping_params = replace( dewarping_params, '"viewMode": "2"', '"hStretch": 1, "viewMode": "VerticalUp"' ) WHERE dewarping_params LIKE '%"viewMode": "2"%';
