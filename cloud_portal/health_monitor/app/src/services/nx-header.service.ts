import { Injectable }             from '@angular/core';
import { NxConfigService }        from './nx-config';
import { Title, Meta }            from '@angular/platform-browser';
import { BehaviorSubject, timer } from 'rxjs';
import { LocalStorageService }    from 'ngx-store';
import { NxSystemsService }       from './systems.service';

@Injectable({
    providedIn: 'root'
})
export class NxHeaderService {
    CONFIG: any;

    // Only to communicate with AJS
    systemIdSubject = new BehaviorSubject(undefined);

    constructor(private config: NxConfigService,
    ) {

        this.CONFIG = this.config.getConfig();
    }


}
