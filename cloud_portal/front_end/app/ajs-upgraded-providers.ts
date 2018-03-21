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

export function accountFactory(i: any) {
    return i.get('account');
}

export const accountServiceProvider = {
    provide: 'account',
    useFactory: accountFactory,
    deps: ['$injector']
};

export function processFactory(i: any) {
    return i.get('process');
}

export const processServiceProvider = {
    provide: 'process',
    useFactory: processFactory,
    deps: ['$injector']
};

export function CONFIGFactory(i: any) {
    return i.get('CONFIG');
}

export const CONFIGProvider = {
    provide: 'CONFIG',
    useFactory: CONFIGFactory,
    deps: ['$injector']
};

export function ngToastFactory(i: any) {
    return i.get('ngToast');
}

export const ngToastProvider = {
    provide: 'ngToast',
    useFactory: ngToastFactory,
    deps: ['$injector']
};

export function systemsFactory(i: any) {
    return i.get('systemsProvider');
}

export const systemsProvider = {
    provide: 'systemsProvider',
    useFactory: systemsFactory,
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

@NgModule({
    providers: [
        accountServiceProvider
    ]
})
export class accountServiceModule {
}

@NgModule({
    providers: [
        processServiceProvider
    ]
})
export class processServiceModule {
}

@NgModule({
    providers: [
        CONFIGProvider
    ]
})
export class CONFIGModule {
}

@NgModule({
    providers: [
        ngToastProvider
    ]
})
export class ngToastModule {
}

@NgModule({
    providers: [
        systemsProvider
    ]
})
export class systemsModule {
}
