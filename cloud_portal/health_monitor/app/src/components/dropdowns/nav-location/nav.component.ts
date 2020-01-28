import {
    Component, OnInit, Input, OnDestroy,
    SimpleChanges, OnChanges
} from '@angular/core';

@Component({
    selector: 'nx-nav-location',
    templateUrl: 'nav.component.html',
    styleUrls: ['nav.component.scss']
})

export class NxNavLocationDropdown implements OnInit, OnDestroy, OnChanges {
    @Input() location: any;

    show: boolean;

    constructor() {

        this.show = false;
    }

    ngOnInit(): void {
    }

    ngOnDestroy() {
    }

    ngOnChanges(changes: SimpleChanges) {
    }
}
