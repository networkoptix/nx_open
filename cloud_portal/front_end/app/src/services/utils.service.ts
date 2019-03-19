import { Injectable }      from '@angular/core';
import { NxConfigService } from './nx-config';

@Injectable({
    providedIn: 'root'
})
export class NxUtilsService {
    CONFIG: any;

    public static sortASC = true;
    public static sortDESC = false;

    constructor(private config: NxConfigService) {
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
}
