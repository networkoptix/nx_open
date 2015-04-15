#!/usr/bin/python
'''nx_statistics.adapters -- Data model adapters for statistics system
'''

import json
import re

from datetime import datetime

class SqlAdapter(object):
    '''SQL adapter to interact with statistics
    '''
    RE_LATEST=re.compile(r'([a-z_]*)_latest')
    QY_LATEST=''.join((
        '(',
        '  SELECT * FROM (',
        '    SELECT * FROM %(table)s ORDER BY timestamp DESC',
        '  ) AS __sorted GROUP BY id',
        ') AS %(table)s_latest'
    ))

    def __init__(self, db, log):
        '''Initializes adapter to work with [sql]
        '''
        self._db = db
        self._cursor = self._db.cursor()
        self._log = log

    def report(self, data):
        '''Saves report [data] into database tables
        '''
        systemId = data['systemId']
        counts = list("%i %s" % (len(d), k) for k, d in data.items() if k != 'systemId')
        self._log.info('Recieved report from system %s, with data: %s' % (
            systemId, ', '.join(counts)
        ))
        for ms in data['mediaservers']:
            if 'storages' in ms:
                for storage in ms['storages']:
                    self._save('storages', storage, systemId)
                del ms['storages']
            self._save('mediaservers', ms, systemId)
        for table in ['clients', 'cameras', 'businessRules', 'licenses']:
            for item in data.get(table, []):
                self._save(table, item, systemId)

    def sqlQuery(self, query):
        '''Performs SQL [query], returns result
        '''
        for table in set(self.RE_LATEST.findall(query)):
            query = query.replace(table + '_latest',
                                  self.QY_LATEST % dict(table=table))
        self._cursor.execute(query)
        cols = list(d[0] for d in self._cursor.description)
        def fetchValue(value):
            if isinstance(value, datetime): return str(value)
            return value
        res = list(list(fetchValue(v) for v in row)
                   for row in self._cursor.fetchall())
        return dict(query=query, columns=cols, data=res)

    def sqlFormat(self, query):
        '''Formats [query] in JSON dict for readability
        '''
        table = self.sqlQuery(query)
        cols, data = table['columns'], table['data']
        rows = ({cols[i]: v for i, v in enumerate(r)} for r in data)
        return json.dumps(list(rows))

    def sqlChart(self, query, t='StackedBar', x=None, y=None):
        '''Makes pygal.[t] by [query] with [x] and [y] values
        '''
        table = self.sqlQuery(query)
        if not x: xi = 0
        else: xi = table['columns'].index(x[0])
        if not y: yis = (i for i in range(len(table['columns'])) if i != xi)
        else: yis = (table['columns'].index(v) for v in y)

        import pygal
        chart = getattr(pygal, t.encode('ascii','ignore'))()
        chart.title = query
        chart.x_labels = map(str, (r[xi] for r in table['data']))
        for yi in yis:
            chart.add(str(table['columns'][yi]),
                      map(float, (r[yi] for r in table['data'])))
        return chart

    def deleteAll(self):
        '''Removes all records from all tables in database
        '''
        tables = self._table_names()
        self._log.warning('Removing all data in all tables: %s' % ', '.join(tables))
        self._cursor.execute('SET SQL_SAFE_UPDATES = 0')
        for tbl in tables:
            self._cursor.execute('DELETE FROM %s' % tbl)
        self._db.commit()

    def _save(self, table, data, systemId):
        data['systemId'] = systemId
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

