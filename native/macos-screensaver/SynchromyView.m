// Synchromy — économiseur d'écran natif pour macOS.
// Le moteur (synchromy.c) calcule chaque image en pixels et la musique ; la vue pose l'image telle quelle à l'écran,
// agrandie sans lissage, et envoie le son à la sortie audio. Aucun moteur web : l'économiseur tourne aussi
// sur l'écran verrouillé. La musique ne sort que de l'écran principal, jamais de la vignette des Réglages.
#import <ScreenSaver/ScreenSaver.h>
#import <QuartzCore/QuartzCore.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#import "synchromy.h"

static NSString * const kModule = @"design.etienne.synchromy.saver";
static const double kFrequence = 48000;
static const UInt32 kImagesParTampon = 1024;

@interface SynchromyView : ScreenSaverView
@end

@implementation SynchromyView {
    Syn *moteur;
    CALayer *toile;
    CGColorSpaceRef espace;
    NSSize tailleMoteur;
    double depart;
    long compte;
    AudioQueueRef file;
    int canaux; float matrice[8*6];   // sortie : nombre de canaux, et la part de chacun des six canaux du moteur
    NSWindow *feuille; NSButton *caseSon; NSSlider *curseurVolume;
}

typedef struct { Syn *s; int canaux; float *matrice; } Sortie;
static Sortie sortieCourante;
static void rappelAudio(void *donnee, AudioQueueRef q, AudioQueueBufferRef b) {
    Sortie *o = (Sortie *)donnee;
    UInt32 n = b->mAudioDataBytesCapacity/(o->canaux*sizeof(float));
    syn_audio_canaux(o->s, (float *)b->mAudioData, (int)n, o->canaux, o->matrice);
    b->mAudioDataByteSize = n*o->canaux*sizeof(float);
    AudioQueueEnqueueBuffer(q, b, 0, NULL);
}

// l'étiquette Core Audio d'une enceinte, dans la langue du moteur
static int etiquette(AudioChannelLabel l) {
    switch (l) {
        case kAudioChannelLabel_Left: case kAudioChannelLabel_LeftWide: case kAudioChannelLabel_LeftCenter: return SYN_L;
        case kAudioChannelLabel_Right: case kAudioChannelLabel_RightWide: case kAudioChannelLabel_RightCenter: return SYN_R;
        case kAudioChannelLabel_Center: case kAudioChannelLabel_Mono: return SYN_C;
        case kAudioChannelLabel_LFEScreen: case kAudioChannelLabel_LFE2: return SYN_LFE;
        case kAudioChannelLabel_LeftSurround: case kAudioChannelLabel_LeftSurroundDirect: return SYN_LS;
        case kAudioChannelLabel_RightSurround: case kAudioChannelLabel_RightSurroundDirect: return SYN_RS;
        case kAudioChannelLabel_RearSurroundLeft: return SYN_LB;
        case kAudioChannelLabel_RearSurroundRight: return SYN_RB;
        case kAudioChannelLabel_CenterSurround: return SYN_CB;
        default: return SYN_AUTRE;
    }
}

// La sortie par défaut du Mac : combien d'enceintes, et lesquelles. Un ampli en HDMI ou une carte son déclarent leur
// disposition (5.1, 7.1…) ; AirPlay et les haut-parleurs du Mac restent en stéréo.
static int dispositionSortie(int *etiquettes, AudioChannelLayout **disposition) {
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioDeviceID dev = 0; UInt32 t = sizeof dev;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &t, &dev) != noErr || !dev) return 2;
    a = (AudioObjectPropertyAddress){ kAudioDevicePropertyPreferredChannelLayout, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyDataSize(dev, &a, 0, NULL, &t) != noErr || t < sizeof(AudioChannelLayout)) return 2;
    AudioChannelLayout *L = malloc(t);
    if (AudioObjectGetPropertyData(dev, &a, 0, NULL, &t, L) != noErr) { free(L); return 2; }
    if (L->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions) {   // une disposition nommée : on la déplie
        UInt32 t2 = 0; AudioChannelLayoutTag tag = L->mChannelLayoutTag;
        if (tag == kAudioChannelLayoutTag_UseChannelBitmap || AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag, sizeof tag, &tag, &t2) != noErr) { free(L); return 2; }
        free(L); L = malloc(t2);
        if (AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag, sizeof tag, &tag, &t2, L) != noErr) { free(L); return 2; }
    }
    int n = (int)MIN(8, L->mNumberChannelDescriptions), surround = 0;
    for (int i = 0; i < n; i++) { etiquettes[i] = etiquette(L->mChannelDescriptions[i].mChannelLabel); if (etiquettes[i] >= SYN_C && etiquettes[i] <= SYN_CB) surround = 1; }
    if (n < 3 || !surround) { free(L); return 2; }
    *disposition = L;
    return n;
}

- (ScreenSaverDefaults *)reglages {
    ScreenSaverDefaults *d = [ScreenSaverDefaults defaultsForModuleWithName:kModule];
    [d registerDefaults:@{ @"son": @YES, @"volume": @0.6 }];
    return d;
}

- (instancetype)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview {
    self = [super initWithFrame:frame isPreview:isPreview];
    if (self) {
        self.animationTimeInterval = 1.0/60;
        self.wantsLayer = YES;
        self.layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
        toile = [CALayer layer];
        toile.magnificationFilter = kCAFilterNearest;   // de vrais gros pixels, comme dans la page
        toile.minificationFilter = kCAFilterNearest;
        toile.contentsGravity = kCAGravityResize;
        toile.actions = @{ @"contents": [NSNull null], @"bounds": [NSNull null], @"position": [NSNull null] };
        [self.layer addSublayer:toile];
        espace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        // macOS récents : l'hôte des économiseurs survit à leur arrêt ; on se range nous-mêmes
        [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(fin:)
                                                                name:@"com.apple.screensaver.willstop" object:nil];
    }
    return self;
}

- (void)dealloc {
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
    [self couperSon];
    syn_detruire(moteur);
    if (espace) CGColorSpaceRelease(espace);
}

// la vignette des Réglages est minuscule ; isPreview n'est pas toujours fiable sous les macOS récents
- (BOOL)apercu { return self.isPreview || self.bounds.size.width < 600; }

- (BOOL)sonore {
    if (self.apercu || ![[self reglages] boolForKey:@"son"]) return NO;
    NSScreen *ecran = self.window.screen;
    return ecran == nil || ecran == NSScreen.screens.firstObject;
}

- (NSSize)tailleVirtuelle {
    NSSize b = self.bounds.size;
    if (b.width < 1 || b.height < 1) return NSMakeSize(1440, 900);
    if (b.width < 1000) { double k = 1280/b.width; return NSMakeSize(1280, b.height*k); }
    return b;
}

- (void)preparer {
    NSSize t = [self tailleVirtuelle];
    if (moteur && fabs(t.width - tailleMoteur.width) < 1 && fabs(t.height - tailleMoteur.height) < 1) return;
    [self couperSon];
    syn_detruire(moteur);
    uint64_t graine = ((uint64_t)arc4random() << 32) | arc4random();
    moteur = syn_creer(t.width, t.height, graine);
    tailleMoteur = t;
    depart = CACurrentMediaTime();
}

- (void)lancerSon {
    if (file || !moteur || ![self sonore]) return;
    // stéréo, ou au-delà : la disposition des enceintes de la sortie décide (5.1, 7.1 par HDMI ou carte son)
    int etiq[8] = { SYN_L, SYN_R }; AudioChannelLayout *disposition = NULL;
    canaux = dispositionSortie(etiq, &disposition);
    syn_activer_son_canaux(moteur, kFrequence, [[self reglages] doubleForKey:@"volume"], canaux);
    syn_sortie(moteur, canaux);
    if (canaux > 2) syn_matrice(canaux, etiq, matrice);
    else { static const float M2[12] = { 1, 0, .7f, 0, .7f, 0,  0, 1, .7f, 0, 0, .7f }; memcpy(matrice, M2, sizeof M2); }
    sortieCourante = (Sortie){ moteur, canaux, matrice };
    AudioStreamBasicDescription f = { 0 };
    f.mSampleRate = kFrequence; f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    f.mChannelsPerFrame = canaux; f.mBitsPerChannel = 32; f.mFramesPerPacket = 1;
    f.mBytesPerFrame = canaux*sizeof(float); f.mBytesPerPacket = f.mBytesPerFrame;
    if (AudioQueueNewOutput(&f, rappelAudio, &sortieCourante, NULL, NULL, 0, &file) != noErr) { file = NULL; free(disposition); return; }
    if (disposition) {   // chaque canal à son enceinte, par son étiquette
        disposition->mNumberChannelDescriptions = (UInt32)canaux;
        UInt32 t = (UInt32)(offsetof(AudioChannelLayout, mChannelDescriptions) + disposition->mNumberChannelDescriptions*sizeof(AudioChannelDescription));
        AudioQueueSetProperty(file, kAudioQueueProperty_ChannelLayout, disposition, t);
        free(disposition);
    }
    for (int i = 0; i < 3; i++) {
        AudioQueueBufferRef b;
        if (AudioQueueAllocateBuffer(file, kImagesParTampon*f.mBytesPerFrame, &b) == noErr) rappelAudio(&sortieCourante, file, b);
    }
    syn_latence(moteur, 2.0*kImagesParTampon/kFrequence + .02);   // l'image attend le son : il sort avec deux tampons de retard
    AudioQueueStart(file, NULL);
    NSLog(@"Synchromy : son en %d canaux", canaux);
}

- (void)couperSon {
    if (!file) return;
    AudioQueueStop(file, true);        // attend la fin du rappel en cours : le moteur peut ensuite disparaître
    AudioQueueDispose(file, true);
    file = NULL;
}

- (void)layout {
    [super layout];
    [CATransaction begin]; [CATransaction setDisableActions:YES];
    toile.frame = self.layer.bounds;
    toile.contentsScale = self.window.backingScaleFactor ?: 2;
    [CATransaction commit];
}

- (void)startAnimation {
    [super startAnimation];
    [self preparer];
    toile.frame = self.layer.bounds;
    [self lancerSon];
}

- (void)stopAnimation {
    [self couperSon];
    [super stopAnimation];
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window && self.isAnimating) [self lancerSon];   // l'écran n'est connu qu'une fois la vue dans sa fenêtre
}

- (void)animateOneFrame {
    if (!moteur) [self preparer];
    if (!file && self.isAnimating && self.window) [self lancerSon];
    if (!syn_image(moteur, (CACurrentMediaTime() - depart)*1000)) return;
    if (++compte % 3600 == 0) NSLog(@"Synchromy : %ld images (%.0f x %.0f)%@", compte, self.bounds.size.width, self.bounds.size.height, file ? @", avec le son" : @"");
    int gw, gh, px;
    const uint32_t *p = syn_pixels(moteur, &gw, &gh, &px);
    CFDataRef donnees = CFDataCreate(NULL, (const UInt8 *)p, (CFIndex)gw*gh*4);
    CGDataProviderRef fournisseur = CGDataProviderCreateWithCFData(donnees);
    CGImageRef image = CGImageCreate(gw, gh, 8, 32, gw*4, espace, kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast,
                                     fournisseur, NULL, false, kCGRenderingIntentDefault);
    [CATransaction begin]; [CATransaction setDisableActions:YES];
    toile.contents = (__bridge id)image;
    [CATransaction commit];
    CGImageRelease(image); CGDataProviderRelease(fournisseur); CFRelease(donnees);
}

- (void)fin:(NSNotification *)n {
    if (self.apercu) return;
    [self couperSon];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ exit(0); });
}

// pour les essais : la dernière image posée, et l'état du son
- (id)imageCourante { return toile.contents; }
- (BOOL)sonActif { return file != NULL; }

/* ---------- options : musique et volume ---------- */
- (BOOL)hasConfigureSheet { return YES; }
- (NSWindow *)configureSheet {
    ScreenSaverDefaults *d = [self reglages];
    NSWindow *f = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 340, 186) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    NSView *v = f.contentView;
    NSTextField *titre = [NSTextField labelWithString:@"Synchromy"];
    titre.font = [NSFont boldSystemFontOfSize:15]; titre.frame = NSMakeRect(20, 146, 300, 22); [v addSubview:titre];
    BOOL fr = [NSLocale.preferredLanguages.firstObject ?: @"fr" hasPrefix:@"fr"];
    NSTextField *note = [NSTextField wrappingLabelWithString:fr ? @"Dessin et musique génératifs, en hommage à Norman McLaren. Conçu par Etienne Mineur. La musique ne sort que de l’écran principal."
                                                                : @"Generative drawing and music, a tribute to Norman McLaren. Designed by Etienne Mineur. Music only plays from the main screen."];
    note.font = [NSFont systemFontOfSize:11]; note.textColor = NSColor.secondaryLabelColor; note.frame = NSMakeRect(20, 92, 300, 50); [v addSubview:note];
    caseSon = [NSButton checkboxWithTitle:fr ? @"Musique" : @"Music" target:nil action:nil];
    caseSon.state = [d boolForKey:@"son"] ? NSControlStateValueOn : NSControlStateValueOff;
    caseSon.frame = NSMakeRect(18, 62, 90, 20); [v addSubview:caseSon];
    curseurVolume = [NSSlider sliderWithValue:[d doubleForKey:@"volume"] minValue:0.05 maxValue:1 target:nil action:nil];
    curseurVolume.frame = NSMakeRect(112, 62, 208, 20); [v addSubview:curseurVolume];
    NSButton *ok = [NSButton buttonWithTitle:@"OK" target:self action:@selector(valider:)];
    ok.keyEquivalent = @"\r"; ok.frame = NSMakeRect(240, 14, 82, 30); [v addSubview:ok];
    feuille = f;
    return f;
}
- (void)valider:(id)s {
    ScreenSaverDefaults *d = [self reglages];
    [d setBool:caseSon.state == NSControlStateValueOn forKey:@"son"];
    [d setDouble:curseurVolume.doubleValue forKey:@"volume"];
    [d synchronize];
    if (feuille.sheetParent) [feuille.sheetParent endSheet:feuille]; else [NSApp endSheet:feuille];
    [feuille orderOut:nil];
}
@end
