def human2bytes(string):
    string_parts = string.split(' ')
    k = {
        'kB': 1024,
        'MB': 1024*1024,
        'GB': 1024*1024*1024
    }[string_parts[1]]

    return int(string_parts[0])*k
