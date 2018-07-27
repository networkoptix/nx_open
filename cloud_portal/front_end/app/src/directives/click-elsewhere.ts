import { Directive, EventEmitter, ElementRef, HostListener, Output, Renderer2 } from '@angular/core';

@Directive({selector: '[clickElsewhere]'})
export class ClickElsewhereDirective {
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
