class License:
    def __init__(self, serial, name, hwid, count, version, brand, expiration, signature2, cls):
        self.serial = serial
        self.name = name
        self.hwid = hwid
        self.count = count
        self.version = version
        self.brand = brand
        self.expiration = expiration
        self.signature2 = signature2
        self.cls = cls

    def to_content(self):
        return '\n'.join(
            [
                f"{k}={v}"
                for k, v in {
                    "NAME": self.name,
                    "SERIAL": self.serial,
                    "HWID": self.hwid,
                    "COUNT": self.count,
                    "CLASS": self.cls,
                    "VERSION": self.version,
                    "BRAND": self.brand,
                    "EXPIRATION": self.expiration,
                    "SIGNATURE2": self.signature2
                }.items()
            ]
        ) + '\n\n'

    @staticmethod
    def parse(content):
        license_dict = dict(
            [license_row[:license_row.find('=')], license_row[license_row.find('=') + 1:]]
            for license_row in content.split('\n')
        )
        return License(
            serial=license_dict['SERIAL'],
            name=license_dict['NAME'],
            hwid=license_dict['HWID'],
            count=int(license_dict['COUNT']),
            version=license_dict['VERSION'],
            brand=license_dict['BRAND'],
            expiration=license_dict['EXPIRATION'],
            signature2=license_dict['SIGNATURE2'],
            cls=license_dict['CLASS']
        )
