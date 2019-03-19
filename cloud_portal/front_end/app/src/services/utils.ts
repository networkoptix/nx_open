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
    static byParam(a, b, param, order) {
        if (a[param] < b[param]) {
            return (order) ? -1 : 1;
        }
        if (a[param] > b[param]) {
            return (order) ? 1 : -1;
        }
        return 0;
    }
}
