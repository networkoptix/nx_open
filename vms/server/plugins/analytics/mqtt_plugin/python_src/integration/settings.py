## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from util.integration_settings import IntegrationSettings
from util.integration_settings import SettingsSectionModel as M
from util.integration_settings import SettingsItemModel as I
from util.integration_settings import SettingsItemModelType as T


def get_settings_model():
    return IntegrationSettings(
        {
            'mqtt': M('MQTT Settings', [
                I(name='address', caption="MQTT Server address"),
                I(name='port', caption="MQTT Server port", type=T.INTEGER, default_value=8883),
                I(name="use_tls", caption="Use TLS", type=T.BOOLEAN, default_value=True),
                I(name='username', caption="MQTT Username"),
                I(name='password', caption="MQTT Password", type=T.PASSWORD),
                I(name='client_id', caption="MQTT Client Id"),
                I(
                    name='topics',
                    caption="MQTT Topics",
                    description="Comma-separated list of topics to which plugin is subscribed")]),
            'http_server': M('HTTP Server Settings', [
                I(name='address', caption="HTTP Server Address To Bind", default_value="0.0.0.0"),
                I(
                    name='port',
                    caption="HTTP Server Port To Bind",
                    default_value=5000,
                    type=T.INTEGER)]),
            'vms_settings': M('Server Events Settings', [
                I(
                    name='server_address',
                    caption="VMS Server Address",
                    default_value='https://localhost'),
                I(
                    name='server_port',
                    caption="VMS Server Port",
                    default_value=7001,
                    type=T.INTEGER),
                I(name='username', caption="VMS User"),
                I(name='password', caption="VMS User Password", type=T.PASSWORD)]),
            'misc': M('Misc Settings', [
                I(
                    name='log_level',
                    caption="MQTT Plugin Log Level",
                    default_value='INFO')]),
        })
