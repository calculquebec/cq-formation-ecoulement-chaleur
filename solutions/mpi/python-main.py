#!/usr/bin/env python

from PIL import Image
import numpy as np
import sys
from mpi4py import MPI  # MPI.Init() implicite


ctc_t = np.float32
BRUIT = ctc_t(6.4 / 256)  # 6.4 unités de la résolution de 8 bits
SEUIL_CONVERGENCE = ctc_t(0.5 / 256)  # 0.5 unité par pixel
NB_MAX_ITER = 5000  # Limiter le temps de calcul

# Indices pour les trois canaux CTC
CHALEUR, TEMPERATURE, CONDUCTION = 0, 1, 2


def separation(n, rank=0, nranks=1):
    """
    Calculer l'indice de séparation de l'espace de calcul

    n - taille de l'espace de calcul
    rank - rang du processus dans le communicateur
    nranks - nombre de processus dans le communicateur

    Retourne: un nombre pair
    """

    return n * rank // (nranks * 2) * 2


def un_pas_de_temps(carte_gpu, rank, nranks):
    """
    Effectuer une itération d'écoulement de chaleur sur toute la grille

    carte_gpu - numpy.ndarray à 3 dimensions, 3 couches pour la 3e dimension
    rank - rang du processus dans le communicateur
    nranks - nombre de processus dans le communicateur

    Retourne: La différence de température moyenne
    """

    assert len(carte_gpu.shape) == 3, "la grille n'est pas 3D"
    assert carte_gpu.shape[2] == 3, "la 3e dimension n'a pas 3 couches"

    somme_delta = ctc_t(0.)

    # Laisser faire la marge de 1 pixel, forcer un nombre pair de lignes
    debut = 1 + separation(carte_gpu.shape[0] - 2, rank, nranks)
    fin = 1 + separation(carte_gpu.shape[0] - 2, rank+1, nranks)

    # Converge plus vite si on traite en damier (une couleur à la fois)
    for diagonale in [[(0, 0), (1, 1)], [(0, 1), (1, 0)]]:
        for i, j in diagonale:
            chaleur = carte_gpu[
                debut + i: fin + i: 2,
                1 + j: -1: 2,
                CHALEUR
            ]
            ancienne_temp = carte_gpu[
                debut + i: fin + i: 2,
                1 + j: -1: 2,
                TEMPERATURE
            ]
            conduction = carte_gpu[
                debut + i: fin + i: 2,
                1 + j: -1: 2,
                CONDUCTION
            ]

            nouvelle_temp = np.maximum(
                chaleur,
                (
                    carte_gpu[debut+i-1: fin+i-1: 2, 1+j: -1: 2, TEMPERATURE] +
                    carte_gpu[debut+i:   fin+i:   2,   j: -2: 2, TEMPERATURE] +
                    carte_gpu[debut+i:   fin+i:   2, 2+j::    2, TEMPERATURE] +
                    carte_gpu[debut+i+1: fin+i+1: 2, 1+j: -1: 2, TEMPERATURE]
                ) / 4 + BRUIT
            )
            delta_temp = np.multiply(conduction, nouvelle_temp - ancienne_temp)

            carte_gpu[debut+i: fin+i: 2, 1+j: -1: 2, TEMPERATURE] += delta_temp
            somme_delta += np.sum(np.absolute(delta_temp))

        # Envoyer notre ligne du haut et recevoir celle du voisin
        envoi_haut = MPI.COMM_WORLD.isend(
            carte_gpu[debut, :, TEMPERATURE],
            (rank + nranks - 1) % nranks,
            123
        )
        recep_haut = MPI.COMM_WORLD.irecv(
            source=(rank + nranks + 1) % nranks, tag=123)

        # Envoyer notre ligne du bas et recevoir celle du voisin
        envoi_bas = MPI.COMM_WORLD.isend(
            carte_gpu[fin - 1, :, TEMPERATURE],
            (rank + nranks + 1) % nranks,
            789
        )
        recep_bas = MPI.COMM_WORLD.irecv(
            source=(rank + nranks - 1) % nranks, tag=789)

        # Compléter les envois
        envoi_haut.wait()
        carte_gpu[fin, :, TEMPERATURE] = recep_haut.wait()
        envoi_bas.wait()
        carte_gpu[debut - 1, :, TEMPERATURE] = recep_bas.wait()

    # Calculer la différence totale
    world_delta = MPI.COMM_WORLD.allreduce(somme_delta)

    return world_delta / (carte_gpu.shape[0] * carte_gpu.shape[1])


def normaliser_couleur(temperatures, minmax):
    """
    Normaliser la température selon les températures minimale et maximale.
    Convertir cette valeur de 0..1 en couleur sur un dégradé de noir, à bleu,
    à magenta, à rouge, à jaune et à blanc, selon une courbe de Bézier.

    temperatures - Températures à normaliser
    minmax - Températures minimale et maximale mesurées

    Retourne: numpy.ndarray à 3 dimensions pour les pixels de l'image finale
    """

    # Normalisation des températures selon t_min et t_max
    t = (temperatures - minmax[0]) / (minmax[1] - minmax[0])

    # Créer une image RGB pour chaque couleur
    images = []
    couleurs = [
       (0,     0,   0),  # Noir
       (0,     0, 255),  # Bleu
       (255,   0, 255),  # Magenta
       (255,   0,   0),  # Rouge
       (255, 255,   0),  # Jaune
       (255, 255, 255)   # Blanc
    ]

    for couleur in couleurs:
        image = np.ndarray(shape=(t.shape[0], t.shape[1], 3))

        # Remplir l'image avec la couleur courante
        for canal in range(3):
            image[:, :, canal] = couleur[canal]

        # Ajouter cette image à la liste
        images.append(image)

    # Calcul itératif de la courbe de Bézier dans l'espace des couleurs
    for iter in range(1, len(images)):
        for i in range(len(images) - iter):
            for canal in range(3):
                images[i][:, :, canal] += np.multiply(
                    t,
                    images[i + 1][:, :, canal] - images[i][:, :, canal]
                )

    return np.uint8(images[0])


def main():
    """
    Programme principal
    """

    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    nranks = comm.Get_size()

    if len(sys.argv) < 2:
        if rank == 0:
            sys.exit(f'Usage: {sys.argv[0]} fichier.png')
        else:
            sys.exit(None)

    try:
        # Charger l'image
        png = np.array(Image.open(sys.argv[1]))

        # Tranformer les pixels RGB en triplets CTC
        carte_gpu = png.astype(ctc_t)
        carte_gpu[:, :, CONDUCTION] /= 256
    except Exception as e:
        if rank == 0:
            sys.exit(f'Erreur: {e}')
        else:
            sys.exit(None)

    # Boucle principale
    delta_temp = SEUIL_CONVERGENCE + 1.
    nb_iter = 0

    while (delta_temp > SEUIL_CONVERGENCE) and (nb_iter < NB_MAX_ITER):
        delta_temp = un_pas_de_temps(carte_gpu, rank, nranks)
        nb_iter += 1

    # Récupération des données
    if rank == 0:
        for r in range(1, nranks):
            debut = 1 + separation(carte_gpu.shape[0] - 2, r, nranks)
            fin = 1 + separation(carte_gpu.shape[0] - 2, r+1, nranks)
            carte_gpu[debut:fin, :, TEMPERATURE] = comm.recv(source=r, tag=456)
    else:
        debut = 1 + separation(carte_gpu.shape[0] - 2, rank, nranks)
        fin = 1 + separation(carte_gpu.shape[0] - 2, rank+1, nranks)
        comm.send(carte_gpu[debut:fin, :, TEMPERATURE], 0, 456)

    if rank == 0:
        # Calcul et affichage de statistiques
        minmax = (
            carte_gpu[:, :, TEMPERATURE].min(),
            carte_gpu[:, :, TEMPERATURE].max()
        )
        print(
            f'Itération #{nb_iter},',
            f'ajustement moyen = {delta_temp * 256} / 256,',
            f't_min = {minmax[0]}, t_max = {minmax[1]}'
        )

        try:
            # Tranformer les températures en pixels RGB
            png = normaliser_couleur(carte_gpu[:, :, TEMPERATURE], minmax)

            # Enregistrer l'image résultante
            Image.fromarray(png, 'RGB').save("resultat.png")
        except Exception as e:
            sys.exit(f'Erreur: {e}')

    MPI.Finalize()


if __name__ == '__main__':
    main()
