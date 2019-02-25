import {
    Component, Input, Output,
    EventEmitter, forwardRef, OnInit, ViewEncapsulation
} from '@angular/core';
import {
    NG_VALUE_ACCESSOR, ControlValueAccessor,
    FormControl, Validator
} from '@angular/forms';

/* Usage
<nx-radio name="groupName" id="groupID"
      [(ngModel)]="user.remember_me"
      (click)?="onClick($event)"
      value="SOME_VALUE"
      disabled?>LABEL
</nx-radio>
*/

@Component({
    selector: 'nx-radio',
    templateUrl: 'radio.component.html',
    styleUrls: ['radio.component.scss'],
    providers: [
        {
            provide: NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxRadioComponent),
            multi: true
        }
    ],
    encapsulation: ViewEncapsulation.None
})
export class NxRadioComponent implements OnInit, ControlValueAccessor, Validator {
    @Input() componentId: string;
    @Input() name: string;
    @Input() label: string;
    @Input() value: string;
    @Input() disabled: any;
    @Output() onClick = new EventEmitter<string>();

    private state: string;
    private _value: any;    // ngModel representation
    private _rbxStates = {
        rbFalse: 'unchecked',
        rbTrue: 'checked',
        rbDisabled: 'disabled',
        rbOrElse: 'tristate'
    };

    // the method set in registerOnChange to emit changes back to the form
    private propagateChange = (_: any) => {
    };

    // validates the form, returns null when valid else the validation object
    public validate(c: FormControl) {
        return null; // valid
    }

    ngOnInit() {
        this.disabled = (this.disabled !== undefined);  // optional param
        this.state = this._rbxStates.rbFalse; // 'unchecked'
    }

    /**
     * Write a new value to the element.
     */
    writeValue(value: any) {
        if ((value && this.value === value)) {
            this.state = this._rbxStates.rbTrue; // 'checked'
        } else {
            // clear other radio buttons
            this.state = this._rbxStates.rbFalse; // 'unchecked'
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
    registerOnTouched(fn: () => void): void {}

    changeState() {
        if (this.disabled) {
            return;
        }

        // only one change is possible false -> true
        // on ndModel change if will reset to false
        this.state = this._rbxStates.rbTrue;

        // Propagate component's value attribute (model)
        this.propagateChange(this.value);
        this.onClick.emit(this.value);
    }
}
