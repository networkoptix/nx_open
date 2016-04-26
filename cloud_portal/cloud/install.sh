apt-get install python-pip
apt-get install mysql-server
apt-get install libmysqlclient-dev
apt-get install python-mysqldb

pip install -r requirements.txt

echo "CREATE DATABASE nx_cloud;" | mysql
python manage.py migrate