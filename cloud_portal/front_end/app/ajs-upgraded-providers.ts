import { FactoryProvider, NgModule } from '@angular/core';

// import { cloudApiService } from '../app/scripts/services/cloud_api';
// import { languageService } from '../app/scripts/services/language';
// import { uuid2Service } from '../app/scripts/services/angular-uuid2';

export function cloudApiFactory(i: any) {
    return i.get('cloudApi');
}

export const cloudApiServiceProvider = {
    provide: 'cloudApiService',
    useFactory: cloudApiFactory,
    deps: ['$injector']
};

export function languageFactory(i: any) {
    return i.get('languageService');
}

export const languageServiceProvider = {
    provide: 'languageService',
    useFactory: languageFactory,
    deps: ['$injector']
};

export function uuid2Factory(i: any) {
    return i.get('uuid2');
}

export const uuid2ServiceProvider = {
    provide: 'uuid2Service',
    useFactory: uuid2Factory,
    deps: ['$injector']
};

@NgModule({
    providers: [
        cloudApiServiceProvider
    ]
})
export class cloudApiServiceModule {
}

@NgModule({
    providers: [
        uuid2ServiceProvider
    ]
})
export class uuid2ServiceModule { }

@NgModule({
    providers: [
        languageServiceProvider
    ]
})
export class languageServiceModule { }
