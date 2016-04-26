nohup python manage.py runserver 0.0.0.0:80 &
celery multi start w1 -A notifications worker -l info
# celery multi stop w1 -A notifications worker -l info