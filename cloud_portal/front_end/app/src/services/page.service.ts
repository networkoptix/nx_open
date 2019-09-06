import { Inject, Injectable } from '@angular/core';
import { NxConfigService }    from './nx-config';
import { Title }              from '@angular/platform-browser';

@Injectable({
    providedIn: 'root'
})
export class NxPageService {
    CONFIG: any;

    constructor(private config: NxConfigService,
                private title: Title) {

        this.CONFIG = this.config.getConfig();
    }

    setPageTitle(value: string) {
        this.title.setTitle(value + ' ' + this.CONFIG.cloudName);
    }
}
