#!/usr/bin/python

import json

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

    def sqlQuery(self, query):
        '''Performs SQL [query], returns result
        '''
        self._cursor.execute(query)
        columns = self._cursor.description
        result = []
        for row in self._cursor.fetchall():
            rowDict = {}
            for index, value in enumerate(row):
                name = columns[index][0]
                if name == 'timestamp': value = str(value)
                rowDict[name] = value
            result.append(rowDict)
        return result

    def deleteAll(self):
        '''Removes all records from all tables in database
        '''
        self._log.warning("Removind all data in all tables!");
        self._cursor.execute("SHOW TABLES")
        tableList = list(opt[0] for opt in self._cursor)
        for table in tableList:
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

    def _field_names(self, table):
        self._cursor.execute("SHOW COLUMNS FROM %s" % table)
        return list(opt[0] for opt in self._cursor)

