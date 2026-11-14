# Voltix

Éditeur de schémas électroniques avec simulation, écrit en C et Qt 6, basé sur [ngspice](https://ngspice.sourceforge.io).
Voltix est un logiciel libre destiné aux étudiants et aux amateurs : modules Électronique, Électrotechnique et Électromagnétique.

*Free schematic editor and circuit simulator for electronics, powered by ngspice.*

## Téléchargement

Les versions pour Windows, Linux et macOS sont sur la page [Releases](https://github.com/Letyran015/Voltix/releases/latest).
Sous Windows, lancez `Voltix-Setup.exe`. Voltix propose ensuite d'installer lui-même les mises à jour.

> Windows peut afficher un avertissement SmartScreen tant que l'installateur n'est pas signé :
> cliquez sur *Informations complémentaires*, puis *Exécuter quand même*.

## Compilation

Prérequis : Qt 6.5 ou plus récent, CMake, un compilateur C/C++ et ngspice (voir `.github/workflows/build.yml`).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Licence

Voltix est publié sous licence [MIT](LICENSE).
Les composants tiers (ngspice : BSD 3 clauses ; Qt : LGPL v3, liaison dynamique) sont décrits dans
[`licenses/THIRD_PARTY_NOTICES.txt`](licenses/THIRD_PARTY_NOTICES.txt). Tous les symboles de composants sont dessinés pour Voltix.

## Politique de confidentialité (Privacy)

Voltix ne collecte, ne stocke et ne transmet aucune donnée personnelle. Les schémas restent sur votre ordinateur.
Le seul accès réseau est la vérification des mises à jour, qui interroge l'API publique de GitHub
(`api.github.com`) et, si vous l'acceptez, télécharge l'installateur depuis GitHub. Cette vérification peut être
désactivée dans les réglages.

*Voltix does not collect, store or transmit any personal data. The only network access is the update check, which
queries the public GitHub API and, with your consent, downloads the installer from GitHub.*

## Signature du code

*Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org).*
(À conserver dans cette page uniquement si la demande de signature est acceptée.)
