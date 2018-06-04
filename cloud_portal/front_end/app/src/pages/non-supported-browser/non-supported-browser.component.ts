import {
    Component, OnInit
} from '@angular/core';


@Component({
    selector: 'non-supported-browser-component',
    templateUrl: 'non-supported-browser.component.html'
})

export class NonSupportedBrowserComponent implements OnInit {

    constructor() {
    }

    ngOnInit(): void {
    }

}

