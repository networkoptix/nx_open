import {
    Component,
    OnInit,
    Input,
    Inject
} from '@angular/core';

@Component({
    selector: 'nx-release',
    templateUrl: 'release.component.html',
})
export class ReleaseComponent implements OnInit {
    @Input() release: any;

    constructor(@Inject('languageService') private language: any,
                @Inject('configService') private configService: any) {
    }

    ngOnInit(): void {
    }
}

