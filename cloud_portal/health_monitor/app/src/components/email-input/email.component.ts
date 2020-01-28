import {
    Component,
    OnInit,
    Input,
    forwardRef,
    ViewEncapsulation
} from '@angular/core';
import { NxConfigService }           from '../../services/nx-config';
import { NxLanguageProviderService } from '../../services/nx-language-provider';
import {
    ControlValueAccessor,
    NG_VALUE_ACCESSOR,
    NG_VALIDATORS,
    Validator,
    FormControl
}                                    from '@angular/forms';

@Component({
    selector   : 'nx-email-input',
    templateUrl: 'email.component.html',
    styleUrls  : ['email.component.scss'],
    providers  : [
        {
            provide    : NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxEmailComponent),
            multi      : true
        },
        {
            provide    : NG_VALIDATORS,
            useExisting: forwardRef(() => NxEmailComponent),
            multi      : true,
        },
    ],
    encapsulation: ViewEncapsulation.None
})
export class NxEmailComponent implements OnInit, ControlValueAccessor, Validator {

    @Input() form: any;
    @Input() componentId: string;
    @Input() lockEmail: boolean;


    CONFIG: any = {};
    LANG: any = {};

    private value: string;

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback = () => {
    };
    private onChangeCallback = (_: any) => {
    };

    // validates the form, returns null when valid else the validation object
    public validate(c: FormControl) {
        if (!c.value) {
            return {
                required: true
            };
        }

        const EMAIL_REGEXP = new RegExp(this.CONFIG.emailRegex);
        if (!EMAIL_REGEXP.test(c.value)) {
            return {
                pattern: true
            };
        }

        return null; // valid
    }

    constructor(private config: NxConfigService,
                private language: NxLanguageProviderService) {
    }

    setValue() {
        // update the form
        this.onChangeCallback(this.value);
        this.form.form.get(this.componentId).markAsUntouched();
    }

    ngOnInit() {
        this.CONFIG = this.config.getConfig();
        this.LANG = this.language.getTranslations();
    }

    /**
     * Write a new (model) value to the element.
     */
    writeValue(value: any) {
        if (value !== null) {
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
