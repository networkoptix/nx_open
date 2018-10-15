import { Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';

/* Usage
<nx-content-block type?=['gray' | empty]>
    <header>
        TITLE
    </header>
    <nx-content-block-section>
        BODY
    </nx-content-block-section>

    <!-- ngFor -->
    <nx-content-block-section>
        <header>
            Section title
        </header>
        Section body
    </nx-content-block-section>

    <nx-content-block-section>
        SECTION without header
    </nx-content-block-section>
    <!-- ngFor -->
</nx-content-block>
*/

@Component({
    selector   : 'nx-content-block',
    templateUrl: 'content-block.component.html',
    styleUrls  : [ 'content-block.component.scss' ],
})
export class NxContentBlockComponent implements OnInit {
    @Input('type') class: string;

    haveHeader: boolean;

    @ViewChild('headerWrapper') headerWrapper: ElementRef;

    constructor() {
        this.haveHeader = true;
    }

    ngOnInit() {
        this.haveHeader = (this.headerWrapper.nativeElement.childNodes[ 0 ].childNodes.length > 0);
    }
}
