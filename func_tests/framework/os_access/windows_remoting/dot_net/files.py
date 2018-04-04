from framework.os_access.windows_remoting.dot_net import get_cim_instance, invoke_cim_instance_method


def get_file_info(protocol, path):
    return get_cim_instance(protocol, 'CIM_DataFile', {'Name': path})


def rename_file(protocol, old_path, new_path):
    return invoke_cim_instance_method(protocol, 'CIM_DataFile', {'Name': old_path}, 'Rename', {'FileName': new_path})
