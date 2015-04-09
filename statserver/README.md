# NX Statistics server

## MySQL database setup

```
  sudo apt-get install mysql-server

  mysql --user=USER --password=PASSWORD <database name> \
    < nx_statistics/sql/database.sql # Create database and tables
```


## CGI Script

```
  sudo apt-get install python-pip
  sudo pip install Flask MySql-python pygal # pygal is optional, for sqlCharts only

  ./statserver_cgi.py # running CGI, edit to configure database connection
```


## NGINX forwarding

```
  location /api {
    proxy_pass http://127.0.0.1:8000;
  }
```

## Wanna see a demo?

Run a script `fake_report.py` a couple times to generate randome reports and
see how statistics works:

```
http://localhost/api/sqlFormat/SELECT parentId, count(id) FROM cameras_latest GROUP BY parentId
http://localhost/api/sqlChart/SELECT cpuArchitecture, count(id) FROM mediaservers GROUP BY cpuArchitecture
```

## Deploy && redeploy

```
sudo pip install wheel

./deploy.sh # build whl, deploy to server and restart uwsgi
```
