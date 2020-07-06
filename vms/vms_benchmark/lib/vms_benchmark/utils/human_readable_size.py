import re

def meminfo_size_to_bytes(string):
    string_parts = string.split(' ')
    k = {
        'kB': 1024,
        'MB': 1024*1024,
        'GB': 1024*1024*1024
    }[string_parts[1]]

    return int(string_parts[0])*k

def df_size_to_bytes(string):
    if re.match(r'^\s*\d+\s*$', string):
        return int(string)

    string_parts = re.match(r'^\s*([\d\.]+)\s*(\w+)?', string)
    metric_units = {
        'k': 1024,
        'M': 1024 * 1024,
        'G': 1024 * 1024 * 1024,
    }

    return int(float(string_parts[1]) * metric_units.get(string_parts[2], 1))
