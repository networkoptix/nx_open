import { Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';

/* Usage
 * <nx-ml-ellipsis config='{"height" : "75", "lineHeight" : "25", "lines" : "3" }' >
 *     Only two things are infinite, the universe and human stupidity,
 *     and I'm not sure about the former.
 *
 *     -- A. Einstein
 * </nx-ml-ellipsis>
*/

@Component({
    selector   : 'nx-ml-ellipsis',
    templateUrl: 'mle.component.html',
    styleUrls  : [ 'mle.component.scss' ],
})
export class NxMultiLineEllipsisComponent implements OnInit {
    @Input() config: string;

    ellipsis: any;

    constructor() {
        // Defaults
        this.ellipsis = {};
        this.ellipsis.viewMore = false;
        this.ellipsis.viewHeight = '75px';
        this.ellipsis.viewLineHeight = 19;
        this.ellipsis.viewLines = 4;
    }

    ngOnInit() {
        if (!this.config) {
            return;
        }

        this.ellipsis.multiLineEllipsis = JSON.parse(this.config);

        if (this.ellipsis.multiLineEllipsis.lines && this.ellipsis.multiLineEllipsis.height) {

            this.ellipsis.viewLines = this.ellipsis.multiLineEllipsis.lines;
            this.ellipsis.viewHeight = this.ellipsis.multiLineEllipsis.height;
            this.ellipsis.viewLineHeight = Math.ceil(this.ellipsis.viewHeight / this.ellipsis.viewLines);

        } else if (this.ellipsis.multiLineEllipsis.lines && this.ellipsis.multiLineEllipsis.lineHeight) {

            this.ellipsis.viewLines = this.ellipsis.multiLineEllipsis.lines;
            this.ellipsis.viewLineHeight = this.ellipsis.multiLineEllipsis.lineHeight;
            this.ellipsis.viewHeight = Math.ceil(this.ellipsis.viewLineHeight * this.ellipsis.viewLines);
        }
    }

    viewMoreDescr() {
        this.ellipsis.viewMore = true;
        this.ellipsis.viewHeight = 'auto';
    }
}
