import { Injectable }                from '@angular/core';
import { NxConfigService }            from './nx-config';
import { BehaviorSubject }           from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class NxAppStateService {
    CONFIG: any;

    footerVisibleSubject = new BehaviorSubject(true);
    headerVisibleSubject = new BehaviorSubject(true);
    readySubject = new BehaviorSubject(false);

    constructor(private config: NxConfigService) {
        this.CONFIG = this.config.getConfig();
    }

    setFooterVisibility(visibile) {
        this.footerVisibleSubject.next(visibile);
    }

    setHeaderVisibility(visibile) {
        this.headerVisibleSubject.next(visibile);
    }

    set ready(ready) {
        this.readySubject.next(ready);
    }

    get ready() {
        return this.readySubject.getValue();
    }
}
