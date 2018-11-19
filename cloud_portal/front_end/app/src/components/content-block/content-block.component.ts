import { Component, ElementRef, Input, OnInit, ViewChild, ViewEncapsulation } from '@angular/core';

/* Usage
<nx-block type?="gray | ...more to come" fixed-height? hoverable?>
    <header>
        TITLE
    </header>
    <nx-section>
        BODY
    </nx-section>

    <!-- ngFor -->
    <nx-section>
        <header>
            Section title
        </header>
        Section body
    </nx-section>

    <nx-section>
        SECTION without header
    </nx-section>
    <!-- ngFor -->

    <footer>
        footer content
    </footer>
</nx-block>
*/

@Component({
    selector   : 'nx-block',
    templateUrl: 'content-block.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls  : [ 'content-block.component.scss' ],
})
export class NxContentBlockComponent implements OnInit {
    @Input('type') type: string;
    @Input('fixed-height') fixedHeight: any;
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

        this.fixedHeight = (this.fixedHeight !== undefined);
        this.hoverable = (this.hoverable !== undefined);
    }
}
