import { Injectable, InjectionToken, FactoryProvider } from '@angular/core';

export const WINDOW = new InjectionToken<Window>('window');

const windowProvider: FactoryProvider = {
    provide: WINDOW,
    useFactory: () => window
};

export const WINDOWS_PROVIDERS = [
    windowProvider
];

