#!/bin/python2

import argparse
import os
import yaml

from environment import zip_files


def create_mediaserver_debug_file(binaries_dir, output_filename):
    pdb_filenames = [
        'mediaserver.pdb',
        'plugins/generic_multicast_plugin.pdb',
        'plugins/generic_rtsp_plugin.pdb',
        'plugins/hanwha_analytics_plugin.pdb',
        'plugins/dw_mtt_analytics_plugin.pdb',
        'plugins/hikvision_analytics_plugin.pdb',
        'plugins/axis_analytics_plugin.pdb',
        'plugins/vca_analytics_plugin.pdb',
        'plugins_optional/image_library_plugin.pdb',
        'plugins_optional/stub_analytics_plugin.pdb',
        'plugins/mjpeg_link_plugin.pdb',
        'plugins/usb_camera_plugin.pdb']
    zip_files(pdb_filenames, binaries_dir, output_filename, mandatory=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_filename = os.path.join(
        args.output, config['server_debug_distribution_name']) + '.zip'
    create_mediaserver_debug_file(
        binaries_dir=config['bin_source_dir'],
        output_filename=output_filename)


if __name__ == '__main__':
    main()
