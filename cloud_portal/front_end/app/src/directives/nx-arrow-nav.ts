import { Directive, ElementRef, HostListener, Input, OnChanges, SimpleChanges } from '@angular/core';

@Directive({selector: '[nxArrowNav]'})
export class NxArrowNavDirective implements OnChanges {

    @Input() nxArrowNav: boolean;

    idx: number;
    elements: any;

    constructor(private _elementRef: ElementRef) {
        this.idx = -1; // position outside so first click will initialize element[0]
    }

    private increaseIdx(limit: number): void {
        this.idx = (this.idx < limit) ? ++this.idx : limit;
    }

    private decreaseIdx(): void {
        this.idx = (this.idx > 0) ? --this.idx : 0;
    }

    ngAfterViewInit() {
        this.elements = this._elementRef.nativeElement.querySelectorAll('.dropdown-item-container');
    }

    ngOnChanges(changes: SimpleChanges) {
        this.idx = -1; // reset index on dropdown open / close
    }

    @HostListener('document:keydown', ['$event'])
    onKeydown(e) {

        // filter events
        if ([38, 40].indexOf(e.keyCode) === -1) {
            return;
        }

        // proceed only if open
        if (this._elementRef.nativeElement.parentElement.className.indexOf('show') > -1) {
            const menuSize = this.elements.length - 1;
            let elm;

            // ArrowDown
            if (e.keyCode == 40) {
                this.increaseIdx(menuSize);
            }

            // ArrowUp
            if (e.keyCode == 38 && this.idx !== -1) { // prevent arrow nav before it was initialized
                this.decreaseIdx();
            }

            elm = this.elements[this.idx];

            if (elm && elm.firstElementChild) {
                elm.firstElementChild.focus();
            }
        }
    }
}
