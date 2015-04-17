#!/bin/sh

. /srv/www/stats.networkoptix.com/statserver/.venv/bin/activate
pip install -I --no-deps $1
sudo service uwsgi-statserver restart
