import { Injectable }                      from '@angular/core';
import { BehaviorSubject, Observable, of } from 'rxjs';
import { NxConfigService }                 from '../../services/nx-config';

@Injectable({
    providedIn: 'root'
})
export class NxHealthService {
    private static ALERTS = 'alertType';
    private static TYPES = 'deviceType';
    private static SERVERS = 'server';

    manifestSubject = new BehaviorSubject(undefined);
    valuesSubject = new BehaviorSubject(undefined);
    alarmsSubject = new BehaviorSubject(undefined);
    systemSubject = new BehaviorSubject(undefined);
    tableReadySubject = new BehaviorSubject(undefined);

    importedData: boolean;
    tableHeaders: any;
    panelParams: any;

    alertsValues: any;
    alertsCount = {
        warning: 0,
        error: 0
    };

    resourceNames = {};

    ready: boolean;

    CONFIG: any;

    constructor(private configService: NxConfigService) {
        this.CONFIG = this.configService.getConfig();
        this.importedData = false;
    }

    get manifest() {
        return this.manifestSubject.getValue();
    }

    set manifest(manifest) {
        this.manifestSubject.next(manifest);
    }

    get values() {
        return this.valuesSubject.getValue();
    }

    set values(values) {
        this.valuesSubject.next(values);
    }

    get alarms() {
        return this.alarmsSubject.getValue();
    }

    set alarms(alarms) {
        this.alarmsSubject.next(alarms);
    }

    get system() {
        return this.systemSubject.getValue();
    }

    set system(system) {
        this.systemSubject.next(system);
    }

    get tableReady() {
        return this.tableReadySubject.getValue();
    }

    set tableReady(tableReady) {
        this.tableReadySubject.next(tableReady);
    }

    pad(n) {
        return n < 10 ? '0' + n : n;
    }

    secondsToTime(seconds, format = 'duration') {
        const timeUnits = ['d', 'h', 'm', 's'];
        const timeDivisors = {d: 60 * 60 * 24, h: 60 * 60, m: 60, s: 1};
        const timeValues = {d: 0, h: 0, m: 0, s: 0};
        let time = '';

        for (const unit of timeUnits) {
            const val = Math.floor(seconds / timeDivisors[unit]);
            seconds -= val * timeDivisors[unit];
            timeValues[unit] = val;
        }

        if (timeValues.d) {
            time += `${timeValues.d}d `;
        }

        if (format === 'durationDh') {
            time += `${timeValues.h}h`;
        } else if (format === 'updateTime') {
            if (timeValues.h) {
                time += `${timeValues.h}h `;
            }
            time += `${timeValues.m}m`;
        } else {
            time += `${this.pad(timeValues.h)}:${this.pad(timeValues.m)}:${this.pad(timeValues.s)}`;
        }

        return time;
    }

    formatValue(header, value) {
        function roundInt(val) {
            if (typeof val === 'number') {
                if (Math.abs(val) >= 10) {
                    return Math.round(val);
                } else {
                    return parseFloat(val.toFixed(2));
                }
            }
            return val;
        }

        let retValue = value;
        let formatDisplay = header.format || '';
        if (header.format) {
            const format = header.format;
            const valueFormats = this.CONFIG.healthMonitoring.valueFormats;
            if (valueFormats[format]) {
                retValue = roundInt(retValue * valueFormats[format].multiplier);
                formatDisplay = valueFormats[format].display || header.format;
                retValue = `${retValue} ${formatDisplay}`;
            } else if (format.includes('duration')) {
                retValue = this.secondsToTime(retValue, format);
            } else if (['resource', 'thumbnail'].includes(format)) {
                retValue = this.resourceNames[retValue] || retValue;
            } else if (!['longText', 'shortText', 'text'].includes(format)) {
                console.error(`Format not recognized: ${format}`);
                retValue = roundInt(retValue);
                retValue = `${retValue} ${format}`;
            }
        } else {
            retValue = roundInt(retValue);
        }

        return {
            text: retValue,
            format: header.format || '',
            formatClass: this.CONFIG.healthMonitoring.classFormats[header.format] || 'no-format',
            value
        };
    }

    itemsSearch(values, filter) {
        let items: any = {};

        function filterItem(c, queryTerms) {
            let result;

            queryTerms.forEach(queryTerm => {
                if (queryTerm.indexOf('-') > -1) {
                    // If dash in query -> perform exact match
                    result = (c.searchTags.includes(queryTerm));
                } else {
                    // If no dash in query -> include results with and without dash
                    result = (c.searchTags.replace(/-/g, '').includes(queryTerm));
                }
            });

            return result;
        }

        if (filter.query === '') {
            items = values;
        } else {
            const query = filter.query.toLowerCase();
            const queryTerms = query.trim()
                                    .split(/[\s,\|]+/)
                                    .filter((elm) => {
                                        return elm !== '';
                                    });

            Object.entries(values).forEach(([metric, value]) => {
                if (filterItem(value, queryTerms)) {
                    items[metric] = value;
                }
            });
        }

        return items;
    }

    alertsSearch(values, filter) {
        let alarms;
        let types;
        let servers;

        const typeAlert = filter.selects && filter.selects.find(x => x.id === NxHealthService.ALERTS);
        if (typeAlert !== undefined) {
            alarms = typeAlert.selected;
        }

        const typeTypes = filter.selects && filter.selects.find(x => x.id === NxHealthService.TYPES);
        if (typeTypes !== undefined) {
            types = typeTypes.selected;
        }

        const typeServers = filter.selects && filter.selects.find(x => x.id === NxHealthService.SERVERS);
        if (typeServers !== undefined) {
            servers = typeServers.selected;
        }

        return values.filter(alert => {
            if (servers && servers.value !== '0' && alert._.server.id !== servers.value) {
                return false;
            }

            if (types && types.value !== '0' && alert._.type.text !== types.value) {
                return false;
            }

            return !(alarms && alarms.value !== '0' && alert._.alarm.icon !== alarms.value);
        });
    }

    findEntityName(entity) {
        if (entity._ && entity._.name) {
            return entity._.name.text;
        } else if (entity.info && entity.info.name) {
            return entity.info.name.text;
        } else {
            return '−';
        }
    }
}
