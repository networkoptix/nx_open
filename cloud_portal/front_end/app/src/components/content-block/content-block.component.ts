import {
    AfterViewInit,
    Component, ElementRef, Inject, Input, OnInit, Renderer2, ViewChild
}                                  from '@angular/core';
import { NgbActiveModal }          from '@ng-bootstrap/ng-bootstrap';
import { DOCUMENT }                from '@angular/common';
import { NxModalGenericComponent } from '../../dialogs/generic/generic.component';

/* Usage
<nx-content-block type?=['gray' | empty]>
    <div card-header>
        TITLE
    </div>
    <div card-body>
        BODY
    </div>

    <!-- ngFor -->
    <div section-title>
        Section title
    </div>
    <div section-content>
        SECTION
    </div>
    <!-- ngFor -->
</nx-content-block>
*/

@Component({
    selector   : 'nx-content-block',
    templateUrl: 'content-block.component.html',
    styleUrls  : [ 'content-block.component.scss' ],
})
export class NxContentBlockComponent implements OnInit, AfterViewInit {
    @Input('type') class: string;

    haveHeader: boolean;
    header: any;

    @ViewChild('headerWrapper') headerWrapper: ElementRef;

    constructor() {
        this.header = true;
    }

    ngAfterViewInit() {
        // this.headerWrapper.nativeElement.childNodes[0] -> H3 element
        this.haveHeader = (this.headerWrapper.nativeElement.childNodes[0].childNodes.length > 0);
    }

    ngOnInit() {

    }
}
