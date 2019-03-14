import { Directive, ElementRef, HostListener, Input } from '@angular/core';

@Directive({selector: '[nxArrowNav]'})
export class NxArrowNavDirective {

    @Input() nxArrowNav: boolean;

    //idx: number;

    constructor(private _elementRef: ElementRef) {
        //this.idx = -1; // position outside so first click will initialize element[0]
    }

    private static increase(idx: number, limit: number): number {
        idx = (idx < limit) ? ++idx : limit;
        return idx;
    }

    private static decrease(idx: number): number {
        idx = (idx > 0) ? --idx : 0;
        return idx;
    }

    // ngOnChanges(changes: SimpleChanges) {
    //     this.idx = -1; // reset index on dropdown open / close
    // }

    @HostListener('document:keydown', ['$event'])
    onKeydown(e) {

        // filter events
        if ([38, 40].indexOf(e.keyCode) === -1) {
            return;
        }

        // proceed only if open
        if (this._elementRef.nativeElement.parentElement.className.indexOf('show') > -1) {
            const elements = this._elementRef.nativeElement.querySelectorAll('.dropdown-item-container');
            let fd_elm = this._elementRef.nativeElement.querySelector(':focus');
            let elm;
            let idx;

            if (fd_elm) {
                fd_elm = fd_elm.parentElement;
            }

            // elements is NodeList and it doesn't implement indexOf
            idx = [].indexOf.call(elements, fd_elm);

            // ArrowDown
            if (e.keyCode == 40) {
                idx = NxArrowNavDirective.increase(idx,elements.length - 1);
            }

            // ArrowUp
            if (e.keyCode == 38 && idx !== -1) { // prevent arrow nav before it was initialized
                idx = NxArrowNavDirective.decrease(idx);
            }

            elm = elements[idx];

            if (elm && elm.firstElementChild) {
                elm.firstElementChild.focus();
            }
        }
    }
}
