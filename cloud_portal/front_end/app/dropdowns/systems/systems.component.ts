import { Component, OnInit, Input, ChangeDetectorRef, Inject } from '@angular/core';
import { NgbDropdownModule }                                   from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'nx-systems',
    templateUrl: './dropdowns/systems/systems.component.html',
    styleUrls: ['./dropdowns/systems/systems.component.scss']
})

export class NxSystemsDropdown implements OnInit {
    @Input() activeSystem: any;
    @Input() systems: any;
    @Input() active: any;

    systemCounter: number;

    constructor(@Inject('languageService') private language: any,
                @Inject('configService') private config: any) {
    }

    trackByFn(index, item) {
        return item.id;
    }

    ngOnInit(): void {

        console.log('systems');
        console.log('activeSystem');

        this.systemCounter = this.systems.length;
    }

    ngOnDestroy() {

    }
}
