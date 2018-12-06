import { Component, OnInit, Input, SimpleChanges, Inject, OnChanges } from '@angular/core';
import { ActivatedRoute }                                             from '@angular/router';
import { Location }                                                   from '@angular/common';
import { Utils }                                                      from '../../utils/helpers';

@Component({
    selector: 'nx-systems',
    templateUrl: 'systems.component.html',
    styleUrls: ['systems.component.scss']
})

export class NxSystemsDropdown implements OnInit, OnChanges {
    @Input() endpoint: any;
    @Input() systems: any;
    @Input() activeSystem: any;

    systemCounter: number;
    active = {
        register: false,
        view: false,
        settings: false
    };
    routeSystemId: string;
    params: any;
    show: boolean;

    constructor(@Inject('languageService') private language: any,
                @Inject('configService') private config: any,
                private location: Location,
                private route: ActivatedRoute) {

        this.show = false;
    }

    trackByFn(index, item) {
        return item.id;
    }

    getUrlFor(sid) {
        let url = '/systems/' + sid;

        if (this.endpoint.view) {
            url += '/view';
        }

        return url;
    }


    ngOnInit(): void {
        // this.updateActive();
        this.systemCounter = this.systems.length;
    }

    ngOnChanges(changes: SimpleChanges) {
        this.endpoint = (changes.endpoint) ? changes.endpoint.currentValue : this.endpoint;
        this.systems = (changes.systems) ? changes.systems.currentValue : this.systems;
        this.activeSystem = (changes.activeSystem) ? changes.activeSystem.currentValue : this.activeSystem;
        this.systemCounter = this.systems.length;
    }
}
