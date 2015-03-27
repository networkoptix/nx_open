#!/usr/bin/python

import json

from pygal import HorizontalStackedBar

class SqlAdapter(object):
    '''SQL adapter to interact with statistics
    '''
    def __init__(self, sql):
        '''Initializes adapter to work with [sql]
        '''
        self._db = sql.connect()
        self._cursor = self._db.cursor()
        self._log = sql.app.logger

    def report(self, data):
        '''Saves report [data] into database tables
        '''
        for ms in data['mediaservers']:
            for s in ms['storages']:
                self._save('storages', s)
            del ms['storages']
            ms['systemId'] = data['systemId']
            self._save('mediaservers', ms)
        for c in data['clients']: self._save('clients', c)
        for c in data['cameras']: self._save('cameras', c)
        return 'Success'

    def sqlQuery(self, query, columns=[]):
        '''Performs SQL [query], returns result
        '''
        for table in self._table_names():
            query = query.replace(table + '_latest',
                "(%s) as %s_latest" % (self._select_latest(table), table))
        self._cursor.execute(query)
        for desc in self._cursor.description:
            columns.append(desc[0])
        result = []
        for row in self._cursor.fetchall():
            rowDict = {}
            for index, value in enumerate(row):
                name = columns[index]
                if name == 'timestamp': value = str(value)
                rowDict[name] = value
            result.append(rowDict)
        return result

    def sqlChart(self, query, x=None, y=None):
        '''Makes pygal HorisontalStackedBar from [query]
        '''
        columns = []
        rows = self.sqlQuery(query, columns)
        x, y = x or columns[0], y or columns[1]
        chart = HorizontalStackedBar()
        chart.title = query
        chart.x_labels = map(str, (r[x] for r in rows))
        chart.add(y, map(float, (r[y] for r in rows)))
        return chart

    def deleteAll(self):
        '''Removes all records from all tables in database
        '''
        self._log.warning("Removind all data in all tables!");
        for table in self._table_names():
            self._cursor.execute("DELETE FROM %s" % table)
        return 'Clear'

    def _save(self, table, data):
        if data.has_key('addParams'):
            for ap in data['addParams']:
                data[ap['name']] = ap['value']
            del data['addParams']
        field_names = self._field_names(table)
        etc = {}
        for name, value in data.items():
            if name not in field_names:
                etc[name] = value
                del data[name]
        data['etc'] = json.dumps(etc)
        query = 'INSERT INTO %s (%s) VALUES (%s)' % (
            table, ', '.join(data.keys()),
            ', '.join('%(' + f + ')s' for f in data.keys()))
        self._cursor.execute(query, data)
        self._db.commit()

    def _table_names(self):
        self._cursor.execute("SHOW TABLES")
        return list(opt[0] for opt in self._cursor)

    def _field_names(self, table):
        self._cursor.execute("SHOW COLUMNS FROM %s" % table)
        return list(opt[0] for opt in self._cursor)

    def _select_latest(self, table):
        inner = "SELECT * FROM %s ORDER BY timestamp DESC" % table
        return "SELECT * FROM (%s) AS __sorted GROUP BY id" % inner
