import { platformBrowserDynamic } from '@angular/platform-browser-dynamic';
import { UpgradeModule } from '@angular/upgrade/static';
import { AppModule } from './app.module';
import { setUpLocationSync } from '@angular/router/upgrade';
// import { Router } from '@angular/router';

platformBrowserDynamic().bootstrapModule(AppModule).then(platformRef => {
    const upgrade = platformRef.injector.get(UpgradeModule) as UpgradeModule;
    upgrade.bootstrap(document.documentElement, ['cloudApp']);
    // platformRef.injector.get(Router).initialNavigation();

    setUpLocationSync(upgrade);
});
