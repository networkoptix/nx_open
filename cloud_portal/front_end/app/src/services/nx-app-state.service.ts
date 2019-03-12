import { Injectable }                from '@angular/core';
import { NxConfigService}            from './nx-config';
import { BehaviorSubject }           from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class NxAppStateService {
    config: any;
    footerVisibleObservable = new BehaviorSubject(true);

    constructor(private _config: NxConfigService) {
        this.config = this._config.getConfig();
        this.footerVisibleObservable.next(this.config.showHeaderAndFooter);
    }

    setFooterVisibility(visibile) {
        this.footerVisibleObservable.next(visibile);
    }
}