import { Component, OnInit, Input, OnDestroy, SimpleChanges, OnChanges } from '@angular/core';
import { Location }                                                      from '@angular/common';
import { ActivatedRoute }                                        from '@angular/router';

@Component({
    selector: 'nx-active-system',
    templateUrl: 'active-system.component.html',
    styleUrls: ['active-system.component.scss']
})

export class NxActiveSystemDropdown implements OnInit, OnDestroy, OnChanges {
    @Input() activeSystem: any;

    active = {
        register: false,
        view: false,
        settings: false
    };
    routeSystemId: string;
    params: any;

    constructor(private location: Location,
                private route: ActivatedRoute,) {

    }

    private isActive(val) {
        const currentPath = this.location.path();

        return (currentPath.indexOf(val) >= 0)
    }

    private updateActive() {
        this.active.register = this.isActive('/register');
        this.active.view = this.isActive('/view');
        this.active.settings = this.routeSystemId && !this.isActive('/view');
    }

    ngOnInit(): void {
        this.updateActive();
    }

    ngOnDestroy() {
    }

    ngOnChanges(changes: SimpleChanges) {
        // this.activeSystem = this.system;
        this.updateActive();
    }
}
