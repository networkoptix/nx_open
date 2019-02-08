UPDATE vms_businessrule
SET action_params = replace( action_params, '"soundUrl":', '"url":' ) WHERE action_params LIKE '%"soundUrl":%';
