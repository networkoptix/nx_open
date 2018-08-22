import {
    Component, Input, Output,
    EventEmitter, forwardRef,
    Optional, Inject, OnInit
} from '@angular/core';
import {
    NG_VALUE_ACCESSOR, ControlValueAccessor,
    NG_VALIDATORS, NG_ASYNC_VALIDATORS,
    FormControl, Validator
} from '@angular/forms';

/* Usage
<nx-checkbox name="remember" id="remember"
      label="{ 'Remember me' | translate}"
      [(ngModel)]="user.remember_me"
      checked?="true"
      (click)?="onClick($event)"
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
        }
    ]
})
export class NxCheckboxComponent implements OnInit, ControlValueAccessor, Validator {
    @Input() label: string;
    @Input() checked: boolean;
    @Output() onClick = new EventEmitter<string>();

    private state: string;
    private value: any;
    private cbxStates = {
        'false': 'unchecked',
        'true': 'checked',
        'bla': 'tristate'
    };

    // the method set in registerOnChange to emit changes back to the form
    private propagateChange = (_: any) => {};

    // validates the form, returns null when valid else the validation object
    public validate(c: FormControl) {
        console.log('validator ->');
        return null; // valid
    }

    ngOnInit() {

    }

    /**
     * Write a new value to the element.
     */
    writeValue(value: any) {
        if (value) {
            console.log('test ->', value);
            this.value = (this.checked !== undefined) ? this.checked : value;
            this.state = this.cbxStates[this.value] || this.cbxStates[value];
        }
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

    changeState() {
        this.value = !this.value;
        this.propagateChange(this.value);
        this.state = this.cbxStates[this.value];

        this.onClick.emit(this.value);
    }
}
