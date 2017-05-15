export CLOUD_PORTAL_CONF_DIR=/opt/networkoptix/
nohup python manage.py runserver 0.0.0.0:8080 &
celery multi stop w1 -A notifications worker -l info
celery multi start w1 -A notifications worker -l info
