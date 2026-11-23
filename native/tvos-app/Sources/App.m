// Synchromy pour Apple TV.
// Le même moteur natif que l'économiseur du Mac (synchromy.c) : la toile dessine seule, toujours différente,
// et sa musique avec elle. L'app empêche l'économiseur de l'Apple TV de prendre la main.
//
// Télécommande : Lecture/Pause coupe ou rend la musique ; le bouton central (ou un clic du pavé) lance un nouveau tableau ;
// Menu (ou Retour) ramène à l'écran d'accueil.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <MediaPlayer/MediaPlayer.h>
#import "synchromy.h"

static const double kFrequence = 48000;
static const double kVolume = .6;

@interface Toile : UIViewController
@end

@implementation Toile {
    Syn *moteur;
    CALayer *calque;
    CADisplayLink *lien;
    AudioQueueRef file;
    CGColorSpaceRef espace;
    double depart;
    BOOL muet;
    UILabel *avis;
    int canaux; float matrice[8*6];
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

// l'étiquette d'une enceinte de la sortie, dans la langue du moteur
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

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;
    espace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    calque = [CALayer layer];
    calque.magnificationFilter = kCAFilterNearest;   // de vrais gros pixels, comme dans la page
    calque.minificationFilter = kCAFilterNearest;
    calque.contentsGravity = kCAGravityResize;
    calque.actions = @{ @"contents": [NSNull null], @"bounds": [NSNull null], @"position": [NSNull null] };
    [self.view.layer addSublayer:calque];

    avis = [[UILabel alloc] init];
    avis.font = [UIFont monospacedSystemFontOfSize:28 weight:UIFontWeightMedium];
    avis.textColor = UIColor.blackColor;
    avis.backgroundColor = [UIColor colorWithRed:.224 green:1 blue:.078 alpha:1];   // vert fluo de la charte
    avis.textAlignment = NSTextAlignmentCenter;
    avis.alpha = 0;
    [self.view addSubview:avis];

    UITapGestureRecognizer *lecture = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(basculerSon)];
    lecture.allowedPressTypes = @[@(UIPressTypePlayPause)];
    [self.view addGestureRecognizer:lecture];
    UITapGestureRecognizer *choix = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(nouveauTableau)];
    choix.allowedPressTypes = @[@(UIPressTypeSelect)];
    [self.view addGestureRecognizer:choix];

    NSNotificationCenter *nc = NSNotificationCenter.defaultCenter;
    [nc addObserver:self selector:@selector(sommeil) name:UISceneWillDeactivateNotification object:nil];
    [nc addObserver:self selector:@selector(reveil) name:UISceneDidActivateNotification object:nil];
    // une autre sortie (ampli branché, HomePod choisis…) : on refait le son pour ses enceintes
    [nc addObserver:self selector:@selector(routeChangee:) name:AVAudioSessionRouteChangeNotification object:nil];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [CATransaction begin]; [CATransaction setDisableActions:YES];
    calque.frame = self.view.bounds;
    [CATransaction commit];
    CGSize b = self.view.bounds.size;
    avis.frame = CGRectMake(b.width/2 - 260, b.height - 150, 520, 60);
    if (!moteur && b.width > 0) {
        uint64_t graine = ((uint64_t)arc4random() << 32) | arc4random();
        moteur = syn_creer(b.width, b.height, graine);
        depart = CACurrentMediaTime();
    }
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self reveil];
    static BOOL credit = NO;   // au lancement, le nom de l'auteur, un instant
    if (!credit) { credit = YES; dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5*NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [self dire:@"Synchromy · Etienne Mineur" duree:3.5]; }); }
}

- (void)reveil {
    if (!moteur) return;
    UIApplication.sharedApplication.idleTimerDisabled = YES;   // l'économiseur de l'Apple TV ne vient pas interrompre la toile
    if (!lien) {
        lien = [CADisplayLink displayLinkWithTarget:self selector:@selector(image)];
        lien.preferredFrameRateRange = CAFrameRateRangeMake(30, 60, 60);
        [lien addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    }
    [self lancerSon];
}

- (void)sommeil {
    [lien invalidate]; lien = nil;
    [self couperSon];
    UIApplication.sharedApplication.idleTimerDisabled = NO;
}

// Le son de l'Apple TV va aussi loin que la sortie le permet. Par HDMI, la box envoie jusqu'à huit canaux en PCM
// (5.1 ou 7.1, caisson compris) à l'ampli ou à la barre de son ; on les demande tous, puis on lit l'étiquette de chaque
// enceinte pour y placer le bon canal. L'app se déclare source multicanale et « en lecture » : sur des HomePod, le système
// peut alors spatialiser le son. Sans enceintes surround, c'est la stéréo d'origine.
- (void)lancerSon {
    if (file || !moteur) return;
    AVAudioSession *session = AVAudioSession.sharedInstance;
    [session setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:nil];
    [session setPreferredSampleRate:kFrequence error:nil];
    if (@available(tvOS 15.0, *)) [session setSupportsMultichannelContent:YES error:nil];
    [session setActive:YES error:nil];
    NSInteger maxi = MIN(8, session.maximumOutputNumberOfChannels);
    if (maxi > 2) [session setPreferredOutputNumberOfChannels:maxi error:nil];
    int n = (int)MIN(8, MAX(2, session.outputNumberOfChannels)), etiq[8], surround = 0, k = 0;
    for (AVAudioSessionPortDescription *port in session.currentRoute.outputs)
        for (AVAudioSessionChannelDescription *c in port.channels) if (k < 8) { etiq[k] = etiquette(c.channelLabel); if (etiq[k] >= SYN_C && etiq[k] <= SYN_CB) surround = 1; k++; }
    if (k < n) {   // pas d'étiquettes : l'ordre habituel (L R C LFE Ls Rs Lrs Rrs)
        static const int ordre[8] = { SYN_L, SYN_R, SYN_C, SYN_LFE, SYN_LS, SYN_RS, SYN_LB, SYN_RB };
        static const int quad[4] = { SYN_L, SYN_R, SYN_LS, SYN_RS };
        for (int i = 0; i < n; i++) etiq[i] = n == 4 ? quad[i] : ordre[i];
        surround = n > 2;
    }
    canaux = surround && n > 2 ? n : 2;
    syn_activer_son_canaux(moteur, kFrequence, muet ? 0 : kVolume, canaux);   // sans effet si déjà branché
    syn_sortie(moteur, canaux);
    if (canaux > 2) syn_matrice(canaux, etiq, matrice);
    else { static const float M2[12] = { 1, 0, .7f, 0, .7f, 0,  0, 1, .7f, 0, 0, .7f }; memcpy(matrice, M2, sizeof M2); }
    sortieCourante = (Sortie){ moteur, canaux, matrice };
    AudioStreamBasicDescription f = { 0 };
    f.mSampleRate = kFrequence; f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    f.mChannelsPerFrame = canaux; f.mBitsPerChannel = 32; f.mFramesPerPacket = 1;
    f.mBytesPerFrame = canaux*sizeof(float); f.mBytesPerPacket = f.mBytesPerFrame;
    if (AudioQueueNewOutput(&f, rappelAudio, &sortieCourante, NULL, NULL, 0, &file) != noErr) { file = NULL; return; }
    if (canaux > 2) {   // chaque canal porte l'étiquette de son enceinte
        size_t t = offsetof(AudioChannelLayout, mChannelDescriptions) + canaux*sizeof(AudioChannelDescription);
        AudioChannelLayout *L = calloc(1, t); L->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions; L->mNumberChannelDescriptions = canaux;
        static const AudioChannelLabel inverse[SYN_NETIQ] = { kAudioChannelLabel_Left, kAudioChannelLabel_Right, kAudioChannelLabel_Center, kAudioChannelLabel_LFEScreen,
            kAudioChannelLabel_LeftSurround, kAudioChannelLabel_RightSurround, kAudioChannelLabel_RearSurroundLeft, kAudioChannelLabel_RearSurroundRight, kAudioChannelLabel_CenterSurround, kAudioChannelLabel_Unused };
        for (int i = 0; i < canaux; i++) L->mChannelDescriptions[i].mChannelLabel = inverse[etiq[i]];
        AudioQueueSetProperty(file, kAudioQueueProperty_ChannelLayout, L, (UInt32)t);
        free(L);
    }
    for (int i = 0; i < 3; i++) {
        AudioQueueBufferRef b;
        if (AudioQueueAllocateBuffer(file, 1024*f.mBytesPerFrame, &b) == noErr) rappelAudio(&sortieCourante, file, b);
    }
    syn_latence(moteur, 2*1024.0/kFrequence + session.outputLatency);   // l'image attend le son de l'ampli
    AudioQueueStart(file, NULL);
    MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo = @{ MPMediaItemPropertyTitle: @"Synchromy", MPMediaItemPropertyArtist: @"Etienne Mineur" };
    MPNowPlayingInfoCenter.defaultCenter.playbackState = MPNowPlayingPlaybackStatePlaying;
    NSLog(@"Synchromy : son en %d canaux (sortie : %ld, au plus %ld)", canaux, (long)session.outputNumberOfChannels, (long)session.maximumOutputNumberOfChannels);
}

- (void)routeChangee:(NSNotification *)n {
    AVAudioSessionRouteChangeReason r = [n.userInfo[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
    if (r != AVAudioSessionRouteChangeReasonNewDeviceAvailable && r != AVAudioSessionRouteChangeReasonOldDeviceUnavailable && r != AVAudioSessionRouteChangeReasonOverride) return;
    dispatch_async(dispatch_get_main_queue(), ^{ if (self->file) { [self couperSon]; [self lancerSon]; } });
}

- (void)couperSon {
    if (!file) return;
    AudioQueueStop(file, true);
    AudioQueueDispose(file, true);
    file = NULL;
}

- (void)image {
    if (!moteur) return;
    if (!syn_image(moteur, (CACurrentMediaTime() - depart)*1000)) return;
    int gw, gh, px;
    const uint32_t *p = syn_pixels(moteur, &gw, &gh, &px);
    CFDataRef donnees = CFDataCreate(NULL, (const UInt8 *)p, (CFIndex)gw*gh*4);
    CGDataProviderRef fournisseur = CGDataProviderCreateWithCFData(donnees);
    CGImageRef im = CGImageCreate(gw, gh, 8, 32, gw*4, espace, (CGBitmapInfo)kCGBitmapByteOrderDefault | (CGBitmapInfo)kCGImageAlphaNoneSkipLast,
                                  fournisseur, NULL, false, kCGRenderingIntentDefault);
    [CATransaction begin]; [CATransaction setDisableActions:YES];
    calque.contents = (__bridge id)im;
    [CATransaction commit];
    CGImageRelease(im); CGDataProviderRelease(fournisseur); CFRelease(donnees);
}

// La langue du système : français si l'Apple TV est en français, anglais sinon.
static BOOL enFrancais(void) { NSString *l = NSLocale.preferredLanguages.firstObject ?: @"fr"; return [l hasPrefix:@"fr"]; }
#define TXT(fr, en) (enFrancais() ? (fr) : (en))

- (void)basculerSon {
    muet = !muet;
    syn_volume(moteur, muet ? 0 : kVolume);
    [self dire:muet ? TXT(@"Musique coupée", @"Music off") : TXT(@"Musique", @"Music on")];
}

- (void)nouveauTableau {
    syn_nouveau_tableau(moteur);
    [self dire:TXT(@"Nouveau tableau", @"New picture")];
}

- (void)dire:(NSString *)texte { [self dire:texte duree:1.2]; }

- (void)dire:(NSString *)texte duree:(double)duree {
    avis.text = texte;
    [UIView animateWithDuration:.2 animations:^{ self->avis.alpha = 1; } completion:^(BOOL f) {
        [UIView animateWithDuration:.4 delay:duree options:0 animations:^{ self->avis.alpha = 0; } completion:nil];
    }];
}

- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
@end

@interface Appli : UIResponder <UIApplicationDelegate>
@end
@implementation Appli
- (BOOL)application:(UIApplication *)a didFinishLaunchingWithOptions:(NSDictionary *)o { return YES; }
- (UISceneConfiguration *)application:(UIApplication *)a configurationForConnectingSceneSession:(UISceneSession *)session options:(UISceneConnectionOptions *)o {
    UISceneConfiguration *c = [[UISceneConfiguration alloc] initWithName:@"Toile" sessionRole:session.role];
    c.delegateClass = NSClassFromString(@"Scene");
    return c;
}
@end

/* tvOS récents : l'app passe par une scène, qui porte la fenêtre */
@interface Scene : UIResponder <UIWindowSceneDelegate>
@property (nonatomic, strong) UIWindow *window;
@end
@implementation Scene
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)o {
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = [Toile new];
    [self.window makeKeyAndVisible];
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(Appli.class)); }
}
