import { Component, ViewChild } from '@angular/core';
import { NgForm }               from '@angular/forms';

@Component({
    selector   : 'sandbox-component',
    templateUrl: 'sandbox.component.html',
    styleUrls  : [ 'sandbox.component.scss' ]
})

export class NxSandboxComponent {
    click: any;
    blah: string;
    group: string;
    agree: boolean;
    show: boolean;
    show5: boolean;
    edit: boolean;
    sections: any;
    options: any;

    submitted = false;

    @ViewChild('testForm') public testForm: NgForm;

    private setupDefaults() {

        this.show = false;
        this.show5 = false;
        this.blah = 'blah1';
        this.group = 'Tsanko';
        this.agree = false;
        this.edit = false;

        this.sections = [
            { title: 'section1', content: 'Some content' },
            { title: 'section2', content: 'Other content' }
        ];

        this.options = [
            { name: 'brand', selected: false, type: 'brand' },
            { name: 'success', selected: true, type: 'success' },
            { name: 'danger', selected: true, type: 'danger' },
            { name: 'warning', selected: false, type: 'warning' },
            { name: 'info', selected: false, type: 'info' },
            { name: 'default', selected: true },
            { name: 'light', selected: true, type: 'light' },
            { name: 'dark', selected: false, type: 'dark' }
        ]

    }

    constructor() {
        this.setupDefaults();
    }

    private touchForm(form) {
        for (const ctrl in form.form.controls) {
            if (form.form.controls.hasOwnProperty(ctrl)) {
                form.form.get(ctrl).markAsTouched();
            }
        }
    }

    onSubmit() {
        if (this.testForm && !this.testForm.valid) {
            // Set the form touched
            this.touchForm(this.testForm);

            return false;
        }

        this.submitted = true;
        window.alert('SUBMIT!');
    }
}

