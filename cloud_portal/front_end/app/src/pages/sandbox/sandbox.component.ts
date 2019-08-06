import { Component, ViewChild } from '@angular/core';
import { NgForm }               from '@angular/forms';

@Component({
    selector   : 'sandbox-component',
    templateUrl: 'sandbox.component.html',
    styleUrls  : ['sandbox.component.scss']
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
    items: any;
    itemsSelected: any;
    filter: any;

    submitted = false;

    @ViewChild('testForm') public testForm: NgForm;

    private setupDefaults() {

        this.show = false;
        this.show5 = false;
        this.blah = 'blah1';
        this.group = 'Tsanko';
        this.agree = false;
        this.edit = false;

        this.filter = {
            query  : '',
            selects: [
                {
                    label: 'Minimum Resolution',
                    items: [
                        { value: '0', name: 'All' },
                        { value: '84480', name: '1CIF' },
                        { value: '168960', name: '2CIF' },
                        { value: '337920', name: 'D1' },
                        { value: '307200', name: 'VGA' },
                        { value: '786432', name: 'SVGA' },
                        { value: '921600', name: '720p' },
                        { value: '1310720', name: '1mp' },
                        { value: '2073600', name: '1080p' },
                        { value: '1920000', name: '2mp' },
                        { value: '3145728', name: '3mp' },
                        { value: '4915200', name: '5mp' },
                        { value: '8000000', name: '8mp' },
                        { value: '10039296', name: '10mp' },
                        { value: '15824256', name: '16mp' }
                    ],
                    selected: undefined
                }
            ],
            multiselects: [
                {
                    label: 'Types',
                    items: [
                        { id: 'Camera', label: 'Camera' },
                        { id: 'Multi-Sensor Camera', label: 'Multi-Sensor Camera' },
                        { id: 'Encoder', label: 'Encoder' },
                        { id: 'DVR', label: 'DVR' },
                        { id: 'Other', label: 'Other' }
                    ],
                    selected: undefined
                }
            ],
            tags   : [
                {
                    label: 'Access Control',
                    value: false
                },
                {
                    label: 'Analytics',
                    value: false
                },
                {
                    label: 'PCIM',
                    value: false
                }
            ]
        };

        this.sections = [
            { title: 'section1', content: 'Some content' },
            { title: 'section2', content: 'Other content' }
        ];

        this.options = [
            { name: 'brand', selected: false, type: 'brand' },
            { name: 'really long name break', selected: false, type: 'brand' },
            { name: 'success', selected: true, type: 'success' },
            { name: 'danger', selected: true, type: 'danger' },
            { name: 'warning', selected: false, type: 'warning' },
            { name: 'info', selected: false, type: 'info' },
            { name: 'default', selected: true }
        ];

        this.items = [
            { label: 'Administrator', id: 'qwerty1' },
            { label: 'Advanced Viewer', id: 'qwerty2' },
            { label: 'Viewer', id: 'qwerty3' },
            { label: 'Live Viewer', id: 'qwerty4' },
            // { label: 'Administrator', id: 'qwerty11' },
            // { label: 'Advanced Viewer', id: 'qwerty12' },
            // { label: 'Viewer', id: 'qwerty13' },
            // { label: 'Live Viewer', id: 'qwerty14' },
            // { label: 'Administrator', id: 'qwerty21' },
            // { label: 'Advanced Viewer', id: 'qwerty22' },
            // { label: 'Viewer', id: 'qwerty23' },
            // { label: 'Live Viewer', id: 'qwerty24' },
        ];

        this.itemsSelected = ['qwerty2', 'qwerty3'];


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

    modelChanged(result: any) {
        // ensure 'change' will be triggered
        this.itemsSelected = [...result];
    }
}

