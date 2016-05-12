sudo sh -c "export CLOUD_PORTAL_CONF_DIR=/opt/networkoptix/; nohup python manage.py runserver 0.0.0.0:80 &"
export CLOUD_PORTAL_CONF_DIR=/opt/networkoptix/;
celery multi start w1 -A notifications worker -l info

# celery multi stop w1 -A notifications worker -l info