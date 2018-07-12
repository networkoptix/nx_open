import { Component, OnInit, Input, SimpleChanges, Inject, OnChanges, OnDestroy } from '@angular/core';
import { ActivatedRoute }                                                        from '@angular/router';
import { Location }                                                              from '@angular/common';

@Component({
    selector: 'nx-systems',
    templateUrl: 'systems.component.html',
    styleUrls: ['systems.component.scss']
})

export class NxSystemsDropdown implements OnInit, OnDestroy, OnChanges {
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

    constructor(@Inject('languageService') private language: any,
                @Inject('configService') private config: any,
                private location: Location,
                private route: ActivatedRoute) {
    }

    private isActive(val) {
        const currentPath = this.location.path();

        return (currentPath.indexOf(val) >= 0);
    }

    private updateActive() {
        this.active.register = this.isActive('/register');
        this.active.view = this.isActive('/view');
        this.active.settings = this.activeSystem && this.activeSystem.id && !this.isActive('/view');
    }

    fireEvent (evt) {
        // alert('EVENT!');
    }

    trackByFn(index, item) {
        return item.id;
    }

    getUrlFor(sid) {
        let url = '/systems/' + sid;

        if (this.active.view) {
            url += '/view';
        }

        return url;
    }

    ngOnInit(): void {
        this.updateActive();
        this.systemCounter = this.systems.length;
    }

    ngOnDestroy() {
    }

    ngOnChanges(changes: SimpleChanges) {
        this.updateActive();

        this.systems = (changes.systems) ? changes.systems.currentValue : this.systems;
        this.activeSystem = (changes.activeSystem) ? changes.activeSystem.currentValue : this.activeSystem;
        this.systemCounter = this.systems.length;
    }
}
