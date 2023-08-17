# Écoulement de chaleur

## Introduction

Ce code utilise un modèle d'écoulement de chaleur basé sur des triplets
chaleur-température-conduction (CTC).
Pour chaque point d'une grille 2D, l'indice de chaleur amène une température
cible minimale, mais les quatre points voisins peuvent imposer une température
moyenne cible plus élevée.
L'indice de conduction de la chaleur (de 0 à 1) est le facteur d'adaptation
au changement de température : une valeur de 0 préservera la température
actuelle, alors qu'une valeur de 1 donnera la nouvelle température cible.

La grille de départ est encodée dans l'image `circuit.png`.
Il s'agit d'une vue 2D d'une carte graphique bidon ; le circuit simplifié
est plus artistique que fonctionnel, mais il permet de voir le comportement de
l'écoulement de la chaleur selon le modèle décrit précédemment.
Chaque pixel RGB (Red, Green, Blue) indique les conditions initiales
d'un point CTC :

* Le rouge est la source de chaleur (C), soit la température cible minimale.
* Le vert est la température initiale (T). Tous les pixels débutent à 22°C.
* Le bleu est l'indice de conduction de la chaleur (C). Les valeurs de 0 à 255
  sont divisées par 256, pour normaliser de 0 à presque 1.

*Note* : autour de la carte graphique, il y a une marge de 8 pixels ayant un
indice de conduction de 0, ce qui permet certaines simplifications.

*Hypothèse* : le modèle se stabilise en accumulant plus de chaleur proche des
sources de chaleur et proche des zones à haute conduction.

## Algorithme

Voici l'algorithme de base d'un pas de temps d'évolution du modèle :

* Pour chaque point CTC de la grille 2D :
  * Noter l'indice de chaleur
  * Noter la température actuelle
  * Noter l'indice de conduction
  * Calculer la température moyenne des quatre points voisins :
    dessus, gauche, droite et dessous
  * Température cible = max(indice de chaleur, température moyenne)
  * Nouvelle température = Température actuelle +
    indice de conduction * (Température cible - Température actuelle)
  * Variation de température = Nouvelle température - Température actuelle

Étant donné qu'il faudra plusieurs pas de temps avant d'atteindre une certaine
convergence du modèle, il faudra une boucle principale limitant le nombre
d'itérations.
De plus, il faudra aussi interrompre la boucle lorsqu'un certain seuil de
convergence sera atteint. En d'autres mots, lorsque la variation moyenne
des températures du modèle est en deçà d'un certain delta-T,
on considère que le modèle se stabilise.

* Tant que le nombre d'itérations est inférieur à la limite ET
  que la variation moyenne des températures est supérieure au seuil :
  * Compter l'itération en cours
  * Calculer un pas de temps du modèle
  * Calculer la moyenne des variations de température

Enfin, on comprend que le modèle peut se calculer du coin supérieur gauche
du modèle jusqu'au point inférieur droit.
Par conséquent, une température moyenne des voisins utiliserait les
températures du nouveau pas de temps pour les points de haut et de gauche,
et du pas de temps précédent pour les points de droite et du bas.
Pour accélérer la convergence, on peut traiter les points de la grille selon
les couleurs d'un damier : traiter toutes les cases blanches et ensuite
toutes les cases noires, le tout dans un même pas de temps du modèle.
Ainsi, la lecture des températures voisines se ferait toujours avec des
valeurs voisines d'une même génération.

## Implémentation

L'image fournie et le code séquentiel suggéré ont été ajustés pour générer
un résultat visuellement intéressant après un temps de calcul raisonnable.
Par exemple, avec une trop faible résolution de température (int8 vs float32),
la convergence pouvait être atteinte trop rapidement.
De plus, un facteur de bruit constant est ajouté à la moyenne pour accélérer
l'accumulation de chaleur dans les points ayant une forte conduction.

## Code C++

### Compilation

```
source source.sh
make
```

### Exécution du binaire

```
./ecoulement circuit.png
```

## Code Python

### Préparation de l'environnement

```
source source.sh
```

### Exécution du script

```
./python-main.py circuit.png
```
