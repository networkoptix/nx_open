import { Component, Input, OnInit } from '@angular/core';

/* Usage
<nx-pre-loader
    type?="page">
</nx-pre-loader>
*/

@Component({
    selector: 'nx-pre-loader',
    templateUrl: 'pre-loader.component.html',
    styleUrls: ['pre-loader.component.scss']
})
export class NxPreLoaderComponent implements OnInit {
    @Input() type: string;

    constructor() {
    }

    ngOnInit() {
        this.type = this.type || '';
    }
}
