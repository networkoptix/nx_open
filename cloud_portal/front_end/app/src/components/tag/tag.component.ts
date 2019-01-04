import { Component, EventEmitter, forwardRef, Input, OnInit, Output, SimpleChange, SimpleChanges } from "@angular/core";
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
    @Input() type: string;
    @Input('value') selected: boolean;
    @Output() onClick = new EventEmitter<boolean>();
    private badgeClass: string;

    constructor(){}

    ngOnInit() {
        this.badgeClass = this.type !== undefined ? `-${this.type}` : '';
        if (this.selected) {
            this.badgeClass = `${this.badgeClass}-selected`;
        }
    }

    ngOnChanges(changes: SimpleChanges) {
        this.selected = changes.selected.currentValue;
        setTimeout(() => {
            if (!this.selected) {
                this.deselectTag();
            } else {
                this.selectTag();
            }
        });
    }

    deselectTag() {
        this.selected = false;
        this.badgeClass = this.type ? `-${this.type}`: '';
        this.changeState(false);
    }

    selectTag() {
        if (!this.selected) {
            this.selected = true;
            this.badgeClass = this.type ? `-${this.type}-selected`: '-selected';
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
