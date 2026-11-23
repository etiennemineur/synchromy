// Synchromy pour Mac — l'outil web complet, embarqué hors ligne dans une fenêtre native.
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface Appli : NSObject <NSApplicationDelegate, NSWindowDelegate, WKNavigationDelegate, WKUIDelegate, WKDownloadDelegate>
@property (strong) NSWindow *fenetre;
@property (strong) WKWebView *vue;
@end

@implementation Appli

- (void)applicationDidFinishLaunching:(NSNotification *)n {
    [self construireMenus];

    WKWebViewConfiguration *cfg = [WKWebViewConfiguration new];
    cfg.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    if (@available(macOS 12.3, *)) cfg.preferences.elementFullscreenEnabled = YES;
    cfg.websiteDataStore = [WKWebsiteDataStore defaultDataStore];   // réglages gardés d'une séance à l'autre
    // la langue de l'app (français ou anglais, selon le système) est transmise à la page ; le choix fait dans la page l'emporte ensuite
    NSString *langue = [NSBundle.mainBundle.preferredLocalizations.firstObject hasPrefix:@"fr"] ? @"fr" : @"en";
    WKUserScript *js = [[WKUserScript alloc] initWithSource:[NSString stringWithFormat:@"window.SYNCHROMY_LANGUE = '%@';", langue]
                                              injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES];
    [cfg.userContentController addUserScript:js];

    NSRect ecran = NSScreen.mainScreen.visibleFrame;
    NSRect cadre = NSMakeRect(0, 0, MIN(1440, ecran.size.width * 0.85), MIN(900, ecran.size.height * 0.85));
    self.fenetre = [[NSWindow alloc] initWithContentRect:cadre
                                               styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                 backing:NSBackingStoreBuffered defer:NO];
    self.fenetre.title = @"Synchromy";
    self.fenetre.titlebarAppearsTransparent = YES;
    self.fenetre.titleVisibility = NSWindowTitleHidden;
    self.fenetre.backgroundColor = NSColor.whiteColor;
    self.fenetre.minSize = NSMakeSize(640, 420);
    self.fenetre.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    self.fenetre.delegate = self;
    self.fenetre.frameAutosaveName = @"Synchromy";
    if (![self.fenetre setFrameUsingName:@"Synchromy"]) [self.fenetre center];

    self.vue = [[WKWebView alloc] initWithFrame:self.fenetre.contentView.bounds configuration:cfg];
    self.vue.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.vue.navigationDelegate = self;
    self.vue.UIDelegate = self;
    self.vue.allowsMagnification = NO;
    [self.vue setValue:@NO forKey:@"drawsBackground"];
    [self.fenetre.contentView addSubview:self.vue];

    NSURL *page = [NSBundle.mainBundle URLForResource:@"synchromy" withExtension:@"html"];
    [self.vue loadFileURL:page allowingReadAccessToURL:page.URLByDeletingLastPathComponent];

    [self.fenetre makeKeyAndOrderFront:nil];
    [self.fenetre makeFirstResponder:self.vue];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)a { return YES; }

#pragma mark Navigation : rien ne sort de l'app, les enregistrements passent par un panneau natif

- (void)webView:(WKWebView *)w decidePolicyForNavigationAction:(WKNavigationAction *)a decisionHandler:(void (^)(WKNavigationActionPolicy))d {
    if (a.shouldPerformDownload) { d(WKNavigationActionPolicyDownload); return; }
    NSURL *u = a.request.URL;
    if (u.isFileURL || [u.scheme isEqualToString:@"about"] || [u.scheme isEqualToString:@"blob"] || [u.scheme isEqualToString:@"data"]) { d(WKNavigationActionPolicyAllow); return; }
    if ([u.scheme hasPrefix:@"http"]) [NSWorkspace.sharedWorkspace openURL:u];
    d(WKNavigationActionPolicyCancel);
}

- (void)webView:(WKWebView *)w decidePolicyForNavigationResponse:(WKNavigationResponse *)r decisionHandler:(void (^)(WKNavigationResponsePolicy))d {
    d(r.canShowMIMEType ? WKNavigationResponsePolicyAllow : WKNavigationResponsePolicyDownload);
}

- (void)webView:(WKWebView *)w navigationAction:(WKNavigationAction *)a didBecomeDownload:(WKDownload *)t { t.delegate = self; }
- (void)webView:(WKWebView *)w navigationResponse:(WKNavigationResponse *)r didBecomeDownload:(WKDownload *)t { t.delegate = self; }

- (void)download:(WKDownload *)t decideDestinationUsingResponse:(NSURLResponse *)r suggestedFilename:(NSString *)nom completionHandler:(void (^)(NSURL *))fin {
    NSSavePanel *p = [NSSavePanel savePanel];
    p.nameFieldStringValue = nom.length ? nom : @"synchromy";
    p.canCreateDirectories = YES;
    [p beginSheetModalForWindow:self.fenetre completionHandler:^(NSModalResponse rep) {
        if (rep != NSModalResponseOK) { fin(nil); return; }
        [NSFileManager.defaultManager removeItemAtURL:p.URL error:nil];   // remplacer si le fichier existe déjà
        fin(p.URL);
    }];
}

- (void)download:(WKDownload *)t didFailWithError:(NSError *)e resumeData:(NSData *)r {
    if (e.code == NSURLErrorCancelled) return;
    NSAlert *al = [NSAlert alertWithError:e];
    [al beginSheetModalForWindow:self.fenetre completionHandler:nil];
}

// La page n'ouvre pas de fenêtre ; par prudence, un lien externe part dans le navigateur.
- (WKWebView *)webView:(WKWebView *)w createWebViewWithConfiguration:(WKWebViewConfiguration *)c forNavigationAction:(WKNavigationAction *)a windowFeatures:(WKWindowFeatures *)f {
    NSURL *u = a.request.URL;
    if ([u.scheme hasPrefix:@"http"]) [NSWorkspace.sharedWorkspace openURL:u];
    return nil;
}

// Si le moteur web s'arrête (mémoire), on recharge plutôt que de laisser une fenêtre vide.
- (void)webViewWebContentProcessDidTerminate:(WKWebView *)w { [w reload]; }

#pragma mark À propos : le nom de l'auteur et l'hommage

- (void)aPropos:(id)s {
    NSMutableParagraphStyle *c = [NSMutableParagraphStyle new]; c.alignment = NSTextAlignmentCenter;
    NSAttributedString *credits = [[NSAttributedString alloc] initWithString:NSLocalizedString(@"Conçu par Etienne Mineur\netienne.design\n\nHommage à Synchromy (1971), de Norman McLaren.", nil)
        attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:11], NSParagraphStyleAttributeName: c, NSForegroundColorAttributeName: NSColor.labelColor }];
    [NSApp orderFrontStandardAboutPanelWithOptions:@{ NSAboutPanelOptionCredits: credits }];
}

#pragma mark Menus

- (void)construireMenus {
    NSMenu *barre = [NSMenu new];
    NSString *nom = @"Synchromy";

    NSMenuItem *mApp = [NSMenuItem new]; [barre addItem:mApp];
    NSMenu *app = [[NSMenu alloc] initWithTitle:nom];
    [app addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"À propos de %@", nil), nom] action:@selector(aPropos:) keyEquivalent:@""];
    [app addItem:NSMenuItem.separatorItem];
    [app addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Masquer %@", nil), nom] action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem *autres = [app addItemWithTitle:NSLocalizedString(@"Masquer les autres", nil) action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    autres.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [app addItemWithTitle:NSLocalizedString(@"Tout afficher", nil) action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [app addItem:NSMenuItem.separatorItem];
    [app addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Quitter %@", nil), nom] action:@selector(terminate:) keyEquivalent:@"q"];
    mApp.submenu = app;

    NSMenuItem *mEd = [NSMenuItem new]; [barre addItem:mEd];
    NSMenu *ed = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Édition", nil)];
    [ed addItemWithTitle:NSLocalizedString(@"Couper", nil) action:@selector(cut:) keyEquivalent:@"x"];
    [ed addItemWithTitle:NSLocalizedString(@"Copier", nil) action:@selector(copy:) keyEquivalent:@"c"];
    [ed addItemWithTitle:NSLocalizedString(@"Coller", nil) action:@selector(paste:) keyEquivalent:@"v"];
    [ed addItemWithTitle:NSLocalizedString(@"Tout sélectionner", nil) action:@selector(selectAll:) keyEquivalent:@"a"];
    mEd.submenu = ed;

    NSMenuItem *mPres = [NSMenuItem new]; [barre addItem:mPres];
    NSMenu *pres = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Présentation", nil)];
    NSMenuItem *pe = [pres addItemWithTitle:NSLocalizedString(@"Plein écran", nil) action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
    pe.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagControl;
    mPres.submenu = pres;

    NSMenuItem *mFen = [NSMenuItem new]; [barre addItem:mFen];
    NSMenu *fen = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Fenêtre", nil)];
    [fen addItemWithTitle:NSLocalizedString(@"Placer dans le Dock", nil) action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [fen addItemWithTitle:NSLocalizedString(@"Réduire/agrandir", nil) action:@selector(performZoom:) keyEquivalent:@""];
    mFen.submenu = fen;
    NSApp.windowsMenu = fen;

    NSApp.mainMenu = barre;
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *a = NSApplication.sharedApplication;
        Appli *d = [Appli new];
        a.delegate = d;
        [a setActivationPolicy:NSApplicationActivationPolicyRegular];
        [a run];
    }
    return 0;
}
