import { Component, OnInit, Input, OnDestroy, SimpleChanges, OnChanges } from '@angular/core';
import { Location }                                                      from '@angular/common';
import { Params, ActivatedRoute }                                        from '@angular/router';
import { NgbDropdownModule }                                             from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'nx-active-system',
    templateUrl: './src/dropdowns/active-system/active-system.component.html',
    styleUrls: ['./src/dropdowns/active-system/active-system.component.scss']
})

export class NxActiveSystemDropdown implements OnInit, OnDestroy, OnChanges {
    // @Input() system: any;
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
        // this.params = this.route.queryParams.subscribe((params: Params) => {
        //     this.routeSystemId = params['systemId'];
        //     console.log(this.route.queryParams);
        // });

        // this.activeSystem = this.system;
        this.updateActive();
    }

    ngOnDestroy() {
        // this.params.unsubscribe();
    }

    ngOnChanges(changes: SimpleChanges) {
        // this.activeSystem = this.system;
        this.updateActive();
    }
}
