import { Component, forwardRef, Input, HostListener, ViewChild } from '@angular/core';
import { NG_VALUE_ACCESSOR, NgForm } from '@angular/forms';
import { NxProcessButtonComponent } from '../process-button/process-button.component';


@Component({
    selector: 'nx-apply',
    templateUrl: 'apply.component.html',
    styleUrls: ['apply.component.scss'],
    providers: [
        {
            provide: NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxApplyComponent),
            multi: true
        }
    ],
})
export class NxApplyComponent {
    @ViewChild(NxProcessButtonComponent, {static: false}) processButton: NxProcessButtonComponent;

    @Input() show: boolean;
    @Input() save: any;
    @Input() discard: any;
    @Input() warn: string;
    @Input() form: NgForm;

    applyVisible: boolean;

    @HostListener('document:keypress', ['$event'])
    handleKeyboardEvent(event: KeyboardEvent) {
        if (event.key === 'Enter' && document.activeElement.tagName === 'INPUT' && this.processButton) {
            this.processButton.checkForm();
        }
    }

    constructor() {
    }
}
