import { Component, ElementRef, Input, OnInit, ViewChild, ViewEncapsulation } from '@angular/core';

/* Usage
<nx-content-block type?=['gray' | empty] hoverable?>
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

    <footer>
        footer content
    </footer>
</nx-content-block>
*/

@Component({
    selector   : 'nx-content-block',
    templateUrl: 'content-block.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls  : [ 'content-block.component.scss' ],
})
export class NxContentBlockComponent implements OnInit {
    @Input('type') class: string;
    @Input('hoverable') hoverable: any;

    haveHeader: boolean;
    haveFooter: boolean;

    @ViewChild('headerWrapper') headerWrapper: ElementRef;
    @ViewChild('footerWrapper') footerWrapper: ElementRef;

    constructor() {
        this.haveHeader = true;
        this.haveFooter = true;
    }

    ngOnInit() {
        this.haveHeader = (this.headerWrapper.nativeElement.childNodes[ 0 ].childNodes.length > 0);
        this.haveFooter = (this.footerWrapper.nativeElement.childNodes.length > 0);

        this.hoverable = (this.hoverable !== undefined);
    }
}
