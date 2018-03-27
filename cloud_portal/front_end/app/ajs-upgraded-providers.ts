import * as angular from 'angular';
import { NgModule } from '@angular/core';

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

export function configServiceFactory(i: any) {
    return i.get('configService');
}

export const configServiceProvider = {
    provide: 'configService',
    useFactory: configServiceFactory,
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

export function systemsServiceFactory(i: any) {
    return i.get('systemsProvider');
}

export const systemsServiceProvider = {
    provide: 'systemsProvider',
    useFactory: systemsServiceFactory,
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
        configServiceProvider
    ]
})
export class configServiceModule {
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
        systemsServiceProvider
    ]
})
export class systemsModule {
}
