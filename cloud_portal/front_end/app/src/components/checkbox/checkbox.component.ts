import {
    Component, Input, Output,
    EventEmitter, forwardRef,
    OnInit, ViewEncapsulation
} from '@angular/core';
import {
    NG_VALUE_ACCESSOR, ControlValueAccessor,
    NG_VALIDATORS, NG_ASYNC_VALIDATORS,
    FormControl, Validator, Validators
} from '@angular/forms';

/* Usage
<nx-checkbox
      name="remember" id="remember"
      [(ngModel)]="user.remember_me"
      (click)?="onClick($event)"
      checked?
      disabled?
      required?>
</nx-checkbox>
*/

@Component({
    selector: 'nx-checkbox',
    templateUrl: 'checkbox.component.html',
    styleUrls: ['checkbox.component.scss'],
    providers: [
        {
            provide: NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxCheckboxComponent),
            multi: true
        },
        {
            provide: NG_VALIDATORS,
            useExisting: forwardRef(() => NxCheckboxComponent),
            multi: true,
        },
    ],
    encapsulation: ViewEncapsulation.None
})
export class NxCheckboxComponent implements OnInit, ControlValueAccessor, Validator {
    @Input() componentId: string;
    @Input() required: any;
    @Input() checked: any;
    @Input() disabled: any;
    @Output() onClick = new EventEmitter<string>();

    private touched: boolean;
    private invalid: boolean;
    private state: string;
    private value: any;
    private cbxStates = {
        'false': 'unchecked',
        'true': 'checked',
        'disabled': 'disabled',
        'or-else': 'tristate'
    };

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
            this.state = this.cbxStates[this.value];
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
        this.state = this.cbxStates[this.value];

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
