/* Synchromy — moteur natif, en C portable.
   Port fidèle du dessin automatique de la version web (etienne.design/tools/synchromy) :
   formes molles et liquide, physique des gouttes, champ des formes, motifs, palettes, trame de fond, cerné.
   Il calcule une image en pixels (une case par « pixel » de Synchromy) ; l'appelant l'agrandit à l'écran. */
#ifndef SYNCHROMY_H
#define SYNCHROMY_H
#include <stdint.h>

typedef struct Syn Syn;

/* largeur et hauteur en points (comme les pixels CSS de la page), graine du hasard */
Syn *syn_creer(double largeur, double hauteur, uint64_t graine);
void syn_detruire(Syn *s);
/* une image d'horloge : t en millisecondes, croissant. Rend 1 si l'image a changé. */
int syn_image(Syn *s, double t_ms);
/* image courante : gw × gh cases, chaque case = 4 octets R, G, B, 255 */
const uint32_t *syn_pixels(const Syn *s, int *gw, int *gh, int *taille_pixel);

/* son : à brancher une fois ; puis le fil audio lit des blocs stéréo entrelacés (gauche, droite) */
void syn_activer_son(Syn *s, double frequence, double volume);
void syn_audio(Syn *s, float *sortie, int images);
/* au-delà de la stéréo : canaux = 2 (stéréo) ou 6 et plus (le moteur calcule alors du 5.1 : FL, FR, C, LFE, SL, SR).
   L'appelant donne l'étiquette de chacune de ses sorties ; syn_matrice en fait une matrice (canaux × 6) pour syn_audio_canaux. */
enum { SYN_L, SYN_R, SYN_C, SYN_LFE, SYN_LS, SYN_RS, SYN_LB, SYN_RB, SYN_CB, SYN_AUTRE, SYN_NETIQ };
void syn_activer_son_canaux(Syn *s, double frequence, double volume, int canaux);
void syn_matrice(int canaux, const int *etiquettes, float *matrice);
void syn_audio_canaux(Syn *s, float *sortie, int images, int canaux, const float *matrice);
/* le pas de musique qui sonne en ce moment, s'il est en avance ou en retard sur l'image (latence de sortie, en secondes) */
void syn_latence(Syn *s, double secondes);
/* le moteur passe en stéréo (2) ou en 5.1 (6 et plus) ; à appeler quand la sortie est arrêtée. */
void syn_sortie(Syn *s, int canaux);
int syn_canaux(const Syn *s);
void syn_volume(Syn *s, double volume);
void syn_nouveau_tableau(Syn *s);

#endif
