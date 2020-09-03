#!/usr/bin/env python

from generic_http_signing_client import GenericHttpSigningClient


def main():
    client = GenericHttpSigningClient('openssl')
    client.init_from_commmand_line()
    return client.send_file()


if __name__ == '__main__':
    ret_code = main()
    exit(ret_code)
