import { Component, EventEmitter, forwardRef, Input, OnInit, Output } from '@angular/core';
import { ControlValueAccessor, FormControl, NG_VALUE_ACCESSOR, Validator } from '@angular/forms';

@Component({
    selector: 'nx-switch',
    templateUrl: 'switch.component.html',
    styleUrls: ['switch.component.scss'],
    providers: [
        {
            provide: NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxSwitchComponent),
            multi: true
        }
    ],
})
export class NxSwitchComponent implements OnInit, ControlValueAccessor, Validator {
    @Input() componentId: string;
    @Input() name: string;
    @Input() required: boolean;
    @Input() value: boolean;
    @Input() disabled: any;
    @Output() onClick = new EventEmitter<boolean>();

    private checked: boolean;
    private invalid: boolean;
    private touched: boolean;

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback = () => {};
    private onChangeCallback = (_: any) => {};

    // validates the form, returns null when valid else the validation object
    public validate(c: FormControl) {
        const err = {
            requiredError: {
                required: true
            }
        };

        this.touched = c.touched;

        if (this.required && !c.value) {
            this.invalid = true;
            return err;
        } else {
            this.invalid = false;
            return null; // valid
        }
    }

    ngOnInit() {
        this.disabled = (this.disabled !== undefined);  // optional param
        this.required = (this.required !== undefined);  // optional param

        setTimeout(() => {
            // set state after model was updated
            if (this.checked !== undefined) {
                this.value = this.checked;
            }
            this.setState();
        });
    }

    /**
     * Write a new (model) value to the element.
     */
    writeValue(value: any) {
        if (value !== null && !this.disabled) {
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

    private setState() {
        // update the form
        this.onChangeCallback(this.value);

        this.onClick.emit(this.value);
    }

    changeState(event) {
        if (this.disabled) {
            return;
        }

        this.onTouchedCallback();
        this.value = !this.value;
        this.setState();
    }

    // Non input elements doesn't have onBlur ... keeping this just for reference
    onBlur() {
        this.onTouchedCallback();
    }
}
