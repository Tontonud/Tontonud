# Tonton Launcher

Un écran d'accueil Android dans le style de TontonOS, écrit sans bibliothèque externe
(environ 600 lignes de Java). Il remplace l'écran d'accueil du téléphone sans le modifier :
on revient à l'ancien quand on veut.

## Fonctions

- **Accueil** : grande horloge, date en français, « Bonjour / Bonsoir » avec votre prénom,
  jusqu'à 8 applis favorites, barre de recherche et dock (Téléphone, Messages, Appareil photo,
  Navigateur).
- **Tiroir d'applis** : glisser vers le haut. Grille alphabétique, recherche instantanée
  (sans tenir compte des accents), Entrée ouvre la première appli trouvée.
- **Appui long sur une appli** : ajouter ou retirer de l'accueil, infos, désinstaller.
- **Glisser vers le bas** au milieu de l'accueil : recherche d'applis avec le clavier.
  Les notifications s'ouvrent en glissant depuis le bord du haut de l'écran (geste du système).

Pas de service d'accessibilité : Google Play Protect bloque l'installation hors Play Store
des applis qui en contiennent (protection anti-fraude).

## Installer sur le téléphone

1. Sur le téléphone, ouvrez dans le navigateur :
   `https://github.com/Tontonud/Tontonud/raw/claude/os-from-scratch-jxsdul/tonton-launcher/TontonLauncher.apk`
2. Ouvrez le fichier téléchargé. Si Android le demande, autorisez votre navigateur à
   **installer des applis inconnues**.
3. Si Play Protect affiche un avertissement (l'appli ne vient pas du Play Store) :
   **Plus de détails → Installer quand même**.
4. Appuyez sur le bouton **Accueil**, choisissez **Tonton Launcher**, puis **Toujours**.
   Sinon : **Paramètres → Applis → Applis par défaut → Appli d'accueil**.

Pour revenir à l'écran d'origine : même menu, choisissez l'ancien launcher (HiOS Launcher sur Tecno).

## Compiler

Sous Ubuntu 24.04 :

```sh
sudo apt install default-jdk android-sdk-platform-23 aapt dalvik-exchange zipalign apksigner
./build.sh        # produit TontonLauncher.apk
```

La clé de signature (`signing.keystore`) est créée au premier build et n'est pas versionnée.
Un APK signé avec une autre clé ne peut pas mettre à jour celui déjà installé : il faut alors
désinstaller l'ancien d'abord.
