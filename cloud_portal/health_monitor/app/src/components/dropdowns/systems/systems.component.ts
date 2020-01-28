import { Component, OnInit, Input, SimpleChanges, OnChanges, Inject } from '@angular/core';
import { NxConfigService }                                            from '../../../services/nx-config';
import { NxUriService }                                               from '../../../services/uri.service';
import { DOCUMENT }                                                   from '@angular/common';

@Component({
    selector: 'nx-systems',
    templateUrl: 'systems.component.html',
    styleUrls: ['systems.component.scss']
})

export class NxSystemsDropdown implements OnInit, OnChanges {
    @Input() endpoint: any;
    @Input() systems: any;
    @Input() activeSystem: any;

    config: any;
    systemCounter: number;
    active = {
        health: false,
        register: false,
        settings: false,
        view: false,
    };
    params: any;
    show: boolean;

    constructor(
        private configService: NxConfigService,
        private uriService: NxUriService,
    ) {

        this.show = false;
        this.config = configService.getConfig();
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.id;
    }

    updateURI(sid) {
        this.show = false;
        let url = '/systems/' + sid;

        if (this.endpoint.view) {
            url += '/view';
        }

        if (this.endpoint.information) {
            url += '/health/';
        }

        this.uriService
            .updateURI(url)
            .then(() => {
                // TODO: Remove this once we retire "VIEW" from AJS
                if (this.endpoint.view) {
                    window.location.href = url;
                }
            });
    }


    ngOnInit(): void {
        this.systemCounter = this.systems.length;
    }

    ngOnChanges(changes: SimpleChanges) {
        this.endpoint = (changes.endpoint) ? changes.endpoint.currentValue : this.endpoint;
        this.systems = (changes.systems) ? changes.systems.currentValue : this.systems;
        this.activeSystem = (changes.activeSystem) ? changes.activeSystem.currentValue : this.activeSystem;
        this.systemCounter = this.systems.length;
    }
}
