import logging
import os
import tempfile
from textwrap import dedent

import click
from pathlib2 import Path

from updates_server.server import UpdatesServer


@click.group(help="Test updates server", epilog=dedent("""
    Replicates functionality of updates.networkoptix.com but
    appends new versions and cloud host information and
    corresponding specific updates links to the existing ones.
    You may specify versions to add as arguments of append_new_versions(...)."""))
@click.option(
    '-d', '--dir', '--data-dir',
    type=click.Path(file_okay=False, dir_okay=True, resolve_path=True),
    default=os.path.join(tempfile.gettempdir(), 'test_updates_server'),
    help="Directory to store downloaded and generated data and serve updates from.")
@click.pass_context
def main(ctx, data_dir):
    ctx.obj = UpdatesServer(Path(data_dir))


@main.command(short_help="Collect data, add versions", help=dedent("""
    Collect actual data from updates.networkoptix.com, add versions and save to the data dir."""))
@click.pass_context
def generate(ctx):
    server = ctx.obj  # type: UpdatesServer
    server.generate_data()


@main.command(short_help="Start HTTP server", help="Serve update metadata and, optionally, archives by HTTP.")
@click.option('--serve-update-archives/--no-serve-update-archives', default=True, show_default=True, help=dedent("""
    Server can serve update files requests. Pre-generated dummy file is given as a response.
    This option might be turned off to test how mediaserver deals with update files absence."""))
@click.option('--range-header', type=click.Choice(['support', 'ignore', 'err']), default='support', show_default=True)
@click.pass_context
def serve(ctx, serve_update_archives, range_header):
    server = ctx.obj  # type: UpdatesServer
    app = server.make_app(serve_update_archives, range_header)
    app.run(port=8080)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    main()
