import { Component, EventEmitter, forwardRef, Input, OnInit, Output } from "@angular/core";
import { ControlValueAccessor, NG_VALUE_ACCESSOR } from "@angular/forms";

@Component({
    selector: 'nx-tag',
    templateUrl: 'tag.component.html',
    styleUrls: ['tag.component.scss'],
    providers: [{
        provide: NG_VALUE_ACCESSOR,
        useExisting: forwardRef(() => NxTagComponent),
        multi: true
    }]
})
export class NxTagComponent implements OnInit, ControlValueAccessor {
    @Input('brand') useBrand: boolean;
    @Input('value') selected: boolean;
    @Output() onClick = new EventEmitter<boolean>();

    constructor(){}

    ngOnInit(){
    }

    deselectTag(){
        this.selected = false;
        this.changeState(false);
    }

    selectTag(){
        if (!this.selected) {
            this.selected = true;
            this.changeState(true)
        }
    }

    // Form control functions
    // The method set in registerOnChange to emit changes back to the form
    private propagateChange = (_: any) => {
    };

    writeValue(value: any) {
        this.selected = value;
    }

    /**
     * Set the function to be called
     * when the control receives a change event.
     */
    registerOnChange(fn) {
        this.propagateChange = fn;
    }

    /**
     * Set the function to be called
     * when the control receives a touch event.
     */
    registerOnTouched(fn: () => void): void {
    }

    changeState(value) {
        // Propagate component's value attribute (model)
        this.propagateChange(value);
        this.onClick.emit(value);
    }

}
