UT_USE_VAGRANT = True

BOX_NAMES = {
    "Box1": "box1j",
    "Box2": "box2j",
    "Nat": "natj",
    "Behind": "behindj",
}
UT_BOX_NAME = "utj"

BOX_IP = {
    'Box1': '192.168.99.8',
    'Box2': '192.168.99.9',
    'Nat': '192.168.99.10',
    'Nat2': '192.168.98.2', 
    'Behind': '192.168.98.3',
}

UT_BOX_IP = '192.168.97.2'

#SKIP_ALL = set(('nx_network_ut', 'cloud_db_ut', 'mediaserver_core_ut',))

