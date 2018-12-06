import * as angular from 'angular';
import { NgModule } from '@angular/core';

function cloudApiFactory(i: any) {
    return i.get('cloudApi');
}

const cloudApiServiceProvider = {
    provide: 'cloudApiService',
    useFactory: cloudApiFactory,
    deps: ['$injector']
};

function languageFactory(i: any) {
    return i.get('languageService');
}

const languageServiceProvider = {
    provide: 'languageService',
    useFactory: languageFactory,
    deps: ['$injector']
};

function accountFactory(i: any) {
    return i.get('account');
}

const accountServiceProvider = {
    provide: 'account',
    useFactory: accountFactory,
    deps: ['$injector']
};

function processFactory(i: any) {
    return i.get('process');
}

const processServiceProvider = {
    provide: 'process',
    useFactory: processFactory,
    deps: ['$injector']
};

function configServiceFactory(i: any) {
    return i.get('configService');
}

const configServiceProvider = {
    provide: 'configService',
    useFactory: configServiceFactory,
    deps: ['$injector']
};

function ngToastFactory(i: any) {
    return i.get('ngToast');
}

const ngToastProvider = {
    provide: 'ngToast',
    useFactory: ngToastFactory,
    deps: ['$injector']
};

function systemsServiceFactory(i: any) {
    return i.get('systemsProvider');
}

const systemsServiceProvider = {
    provide: 'systemsProvider',
    useFactory: systemsServiceFactory,
    deps: ['$injector']
};

function systemServiceFactory(i: any){
    return i.get('system');
}

const systemService = {
    provide: 'system',
    useFactory: systemServiceFactory,
    deps: ['$injector']
};

function authorizationCheckServiceFactory(i: any) {
    return i.get('authorizationCheckService');
}

const authorizationCheckServiceProvider = {
    provide: 'authorizationCheckService',
    useFactory: authorizationCheckServiceFactory,
    deps: ['$injector']
};

function uuid2Factory(i: any) {
    return i.get('uuid2');
}

const uuid2ServiceProvider = {
    provide: 'uuid2Service',
    useFactory: uuid2Factory,
    deps: ['$injector']
};

function localStorageFactory(i: any) {
    return i.get('$localStorage');
}

const localStorageProvider = {
    provide: 'localStorageService',
    useFactory: localStorageFactory,
    deps: ['$injector']
};

function locationProxyFactory(i: any) {
    return i.get('locationProxyService');
}

const locationProxyProvider = {
    provide   : 'locationProxyService',
    useFactory: locationProxyFactory,
    deps      : [ '$injector' ]
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

@NgModule({
    providers: [
        systemService
    ]
})
export class systemModule {
}

@NgModule({
    providers: [
        authorizationCheckServiceProvider
    ]
})
export class authorizationCheckServiceModule {
}

@NgModule({
    providers: [
        localStorageProvider
    ]
})
export class localStorageModule {
}

@NgModule({
    providers: [
        locationProxyProvider
    ]
})
export class locationProxyModule {
}
