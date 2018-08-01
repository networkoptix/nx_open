import { Directive, EventEmitter, ElementRef, HostListener, Output } from '@angular/core';

@Directive({selector: '[nxClickElsewhere]'})
export class NxClickElsewhereDirective {
    @Output()
    clickElsewhere: EventEmitter<any> = new EventEmitter();

    constructor(private _elementRef: ElementRef) {
    }

    @HostListener('document:click', ['$event.target'])
    onMouseClick(targetElement) {
        const clickedInside = this._elementRef.nativeElement.contains(targetElement);

        if (!clickedInside) {
            this.clickElsewhere.emit(null);
        }
    }
}
