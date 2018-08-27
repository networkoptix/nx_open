import { Component, OnInit, OnDestroy, Inject, ViewChild } from '@angular/core';
import { NgForm }                                          from '@angular/forms';

@Component({
    selector: 'sandbox-component',
    templateUrl: 'sandbox.component.html',
    styleUrls: ['sandbox.component.scss']
})

export class NxSandboxComponent implements OnInit, OnDestroy {
    click: any;
    blah: string;
    group: string;
    agree: boolean;
    show: boolean;
    edit: boolean;

    submitted = false;

    @ViewChild('testForm') public testForm: NgForm;

    private setupDefaults() {

        this.show = false;
        this.blah = 'blah1';
        this.group = 'Tsanko';
        this.agree = false;
        this.edit = false;

    }

    constructor() {
        this.setupDefaults();
    }

    private touchForm(form) {
        for (let ctrl in form.form.controls) {
            form.form.get(ctrl).markAsTouched();
        }
    }

    ngOnInit(): void {
    }

    ngOnDestroy() {

    }

    onSubmit() {
        if (this.testForm && !this.testForm.valid) {
            //Set the form touched
            this.touchForm(this.testForm);

            return false;
        }

        this.submitted = true;
        window.alert('SUBMIT!');
    }
}

