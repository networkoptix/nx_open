apt-get install python-pip
apt-get install mysql-server
apt-get install libmysqlclient-dev
apt-get install python-mysqldb

pip install django
pip install djangorestframework
pip install pygments
pip install django-cors-headers
pip install MySQL-python

echo "CREATE DATABASE nx_cloud;" | mysql
python manage.py migrate