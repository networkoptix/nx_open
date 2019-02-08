import { Component, Input, OnInit, SimpleChanges } from '@angular/core';

const noop = () => {};

@Component({
    selector   : 'nx-bool-icon',
    templateUrl: './bool-icon.component.html',
    styleUrls  : ['./bool-icon.component.scss']
})
export class BoolIconComponent implements OnInit {
    @Input() value: string;

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback: () => void = noop;
    private onChangeCallback: (_: any) => void = noop;

    constructor() {
    }

    ngOnInit() {
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.value) {
            this.value = changes.value.currentValue;
        }
    }

    /**
     * Write a new (model) value to the element.
     */
    writeValue(value: any) {
        if (value !== null && typeof value !== 'undefined') {
            this.value = value;
        }
    }

    /**
     * Set the function to be called
     * when the control receives a change event.
     */
    registerOnChange(fn) {
        this.onChangeCallback = fn;
    }

    /**
     * Set the function to be called
     * when the control receives a touch event.
     */
    registerOnTouched(fn: any): void {
        this.onTouchedCallback = fn;
    }

}
