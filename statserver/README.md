# NX Statistics server

## MySQL database setup

```
  sudo apt-get install mysql-server

  mysql --user=USER --password=PASSWORD <database name> \
    < nx_statistics/sql/database.sql # Create database and tables
```


## CGI Scripts

```
  sudo apt-get install python-pip
  sudo pip install Flask MySql-python pygal # pygal is optional, for sqlCharts only

  ./crashserver_cgi.py # running CGI, edit to configure storage path
  ./statserver_cgi.py # running CGI, edit to configure database connection
```


## NGINX forwarding

```
  location /crashserver {
    rewrite ^/[^/]+(/.*)$ $1 break; # strip path prefix
    proxy_pass http://127.0.0.1:8001;
  }
  location /statserver {
    rewrite ^/[^/]+(/.*)$ $1 break; # strip path prefix
    proxy_pass http://127.0.0.1:8002;
  }
```

## Wanna see a demo ststistics?

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
