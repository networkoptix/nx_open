import {
    Component, ElementRef, OnInit, ViewChild, ViewEncapsulation
} from '@angular/core';

/* Usage
<nx-content-block-section>
    <header>
        Section title
    </header>
    Section body
</nx-content-block-section>

<nx-content-block-section>
    SECTION without header
</nx-content-block-section>
*/

@Component({
    selector     : 'nx-content-block-section',
    templateUrl  : 'section.component.html',
    styleUrls    : [ 'section.component.scss' ],
    encapsulation: ViewEncapsulation.None,
})
export class NxContentBlockSectionComponent implements OnInit {

    haveSubheader: boolean;

    @ViewChild('subHeaderWrapper') subHeaderWrapper: ElementRef;

    constructor() {
        this.haveSubheader = true;
    }

    ngOnInit() {
        this.haveSubheader = (this.subHeaderWrapper.nativeElement.childNodes[ 0 ].childNodes.length > 0);
    }
}
