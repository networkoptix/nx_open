import { Directive, EventEmitter, ElementRef, HostListener, Output } from '@angular/core';

@Directive({selector: '[nxClickElsewhere]'})
export class NxClickElsewhereDirective {
    @Output()
    nxClickElsewhere: EventEmitter<any> = new EventEmitter();

    constructor(private _elementRef: ElementRef) {
    }

    @HostListener('document:click', ['$event.target'])
    onMouseClick(targetElement) {
        if (targetElement.type === 'checkbox') {
            // special case for nx-checkbox component not being recognized as internal
            // for nx-multi-select
            return;
        }

        const clickedInside = this._elementRef.nativeElement.contains(targetElement);
        if (!clickedInside) {
            this.nxClickElsewhere.emit(null);
        }
    }
}
