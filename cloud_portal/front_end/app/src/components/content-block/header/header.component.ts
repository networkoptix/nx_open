import {
    Component, OnInit, ViewEncapsulation
} from '@angular/core';

/* Usage

*/

@Component({
    selector     : 'nx-content-block-header',
    templateUrl  : 'header.component.html',
    styleUrls    : [ 'header.component.scss' ],
    encapsulation: ViewEncapsulation.None,
})
export class NxContentBlockHeaderComponent implements OnInit {

    ngOnInit() {
    }
}
