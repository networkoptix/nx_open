import { Inject, Injectable }    from '@angular/core';
import { NxConfigService }       from './nx-config';
import { DOCUMENT }              from '@angular/common';
import { DeviceDetectorService } from 'ngx-device-detector';

@Injectable({
    providedIn: 'root'
})
export class NxUtilsService {
    CONFIG: any;

    public static sortASC = true;
    public static sortDESC = false;

    constructor(private config: NxConfigService,
                private deviceService: DeviceDetectorService,
                @Inject(DOCUMENT) private document: any
    ) {
        this.CONFIG = this.config.getConfig();
    }

    // Sort array of objects
    static byParam(fn, order) {
        return (a, b) => {
            if (fn(a) < fn(b)) {
                return (order) ? -1 : 1;
            }
            if (fn(a) > fn(b)) {
                return (order) ? 1 : -1;
            }
            return 0;
        };
    }

    public keepOriginalOrder = (a, b) => a.key;

    static byResolution(fn, order) {
        return (a, b) => {
            const x = fn(a).map(Number);
            const y = fn(b).map(Number);

            if (x[0] < y[0] || x[1] < y[1]) {
                return (order) ? -1 : 1;
            }
            if (x[0] > y[0] || x[1] > y[1]) {
                return (order) ? 1 : -1;
            }
            return 0;

        };
    }

    static yesNo(bVal) {
        if (bVal === undefined || bVal === null) {
            return 'Unknown';
        }

        return bVal ? 'Yes' : 'No';
    }

    static getRelativeLocation(href) {
        /*
         * Parse url string to:
         *   href,
         *   protocol -> match[1],
         *   host     -> match[2],
         *   hostname -> match[3],
         *   port     -> match[4],
         *   pathname -> match[5],
         *   search   -> match[6],
         *   hash     -> match[7]
         *
         * */
        const match = href.match(/^(https?:)?\/\/(([^:\/?#]*)(?::([0-9]+))?)([\/]{0,1}[^?#]*)(\?[^#]*|)(#.*|)$/);
        if (match) {
            return match[5] + match[6] + match[7];
        } else {
            // href not recognized as valid url
            return href;
        }
    }

    public saveAs(data, filename, type) {
        const a: HTMLAnchorElement = this.document.createElement('a') as HTMLAnchorElement;
        let objectUrl;
        let blob: Blob;

        data = JSON.stringify(data);

        if (this.deviceService.isDesktop()) {
            blob = new Blob([data], {type});
            if (navigator.msSaveOrOpenBlob) {
                navigator.msSaveOrOpenBlob(blob, filename);
                return false;
            }
            objectUrl  = URL.createObjectURL(blob);
            a.href = objectUrl;
        } else {
            a.href = 'data:' + type + ';charset=UTF-8,' + encodeURIComponent(data);
        }

        a.download = filename;

        this.document.body.appendChild(a);
        a.click();
        this.document.body.removeChild(a);

        // revokeObjectURL breaks download on MSEdge and Firefox
        // URL.revokeObjectURL(objectUrl);
    }
}
