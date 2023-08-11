#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include <png.h>


typedef float ctc_t;
const ctc_t BRUIT = 6.4 / 256;  // 6.4 unités de la résolution de 8 bits
const ctc_t SEUIL_CONVERGENCE = 0.5 / 256;  // 0.5 unité par pixel
const unsigned int NB_MAX_ITER = 5000; // Limiter le temps de calcul


/**
 * Classe facilitant la lecture-écriture (Le) de fichiers PNG en RGB
 */
class LePNG: public std::vector<png_color>
{
public:
    LePNG() {
        memset(&entete, 0, sizeof entete);

        entete.format = PNG_FORMAT_RGB;
        entete.version = PNG_IMAGE_VERSION;
    }

    virtual ~LePNG() {
        png_image_free(&entete);
    }

    /**
     * Charger une image d'un fichier PNG - Triple canal (Red, Green, Blue)
     */
    void charger(const std::string & nom_fichier) {
        if (!png_image_begin_read_from_file(&entete, nom_fichier.c_str()))
            throw nom_fichier + " - " + entete.message;

        resize(entete.width * entete.height);

        if (!png_image_finish_read(&entete, NULL, data(), 0, NULL))
            throw nom_fichier + " - " + entete.message;
    }

    /**
     * Enregistrer le résultat dans un fichier PNG
     */
    void enregistrer(const std::string & nom_fichier) {
        if (!png_image_write_to_file(
                &entete, nom_fichier.c_str(), 0, data(), 0, NULL)) {
            throw nom_fichier + " - " + entete.message;
        }
    }

    inline png_uint_32 largeur() const { return entete.width; }
    inline png_uint_32 hauteur() const { return entete.height; }

private:
    png_image entete;
};


/**
 * Affiche un aperçu du canal vert (Green) en ASCII
 * @param ostr   Objet std::ostream tel que std::cout
 * @param modele Le modèle chaleur-température-conduction
 * @return L'objet std::ostream pour des appels à la chaîne
 */
std::ostream& operator<<(std::ostream & ostr, const LePNG & png)
{
    const char ascii[] = " .,!~:;+=xzXZ%#@";
    const auto largeur = png.largeur(), hauteur = png.hauteur();
    const auto sautH = png.largeur() / 80;  // caractères de large
    const auto sautV = sautH * 2;

    ostr << "Taille du modèle: " << largeur << " x " << hauteur << std::endl;

    // Pour chaque bloc complet de taille sautH x sautV
    for (auto i = sautV + hauteur % sautV / 2; i < hauteur; i += sautV) {
        for (auto j = sautH + largeur % sautH / 2; j < largeur; j += sautH) {
            unsigned int vert_tot = 0;

            // Calculer la moyenne des valeurs de vert ( [0, 256[ )
            for (auto ii = i - sautV; ii < i; ++ii) {
                for (auto jj = j - sautH; jj < j; ++jj) {
                    vert_tot += png[ii * largeur + jj].green;
                }
            }

            // Sélectionner un des 16 caractères ASCII (/16 = *16/256)
            ostr << ascii[vert_tot / (sautV * sautH * 16)];
        }

        ostr << std::endl;
    }

    return ostr;
}


/**
 * Triplet (Chaleur, Température, Conduction)
 */
typedef struct {
    ctc_t chaleur;      // Intensité de la source de chaleur
    ctc_t temperature;  // Température en degrés Celsius
    ctc_t conduction;   // Facteur de conduction de chaleur
} CTC;


/**
 * Modèle de grille 2D de valeurs de chaleur, température et conduction
 */
class ModeleCTC: public std::vector<CTC>
{
public:
    ModeleCTC(): larg(0), haut(0) {}

    /**
     * Redimensionner la grille
     */
    void redimensionner(std::size_t largeur, std::size_t hauteur) {
        larg = largeur;
        haut = hauteur;

        resize(larg * haut);
    }

    /**
     * Accès à un triplet (Chaleur, Température, Conduction)
     */
    inline CTC& ctc(std::size_t rangee, std::size_t colonne) {
        return at(rangee * larg + colonne);
    }
    inline const CTC& ctc(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne);
    }

    /**
     * Accès à une composante
     */
    inline ctc_t chaleur(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne).chaleur;
    }
    inline ctc_t temperature(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne).temperature;
    }
    inline ctc_t conduction(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne).conduction;
    }

    inline std::size_t largeur() const { return larg; }
    inline std::size_t hauteur() const { return haut; }

    ctc_t un_pas_de_temps() {
        ctc_t delta_temp = 0.;

        // Convergence accélérée si traité en damier (une couleur à la fois)
        for (auto impair = 0; impair < 2; ++impair) {
            // Laisser faire la marge de 1 pixel
            for (auto i = 1; i < haut - 1; ++i) {
                auto depart = (((i + 1) ^ impair) & 1);  // Damier

                for (auto j = 1 + depart; j < larg - 1; j += 2) {
                    auto conduct = conduction(i, j);
                    auto ancienne_temp = temperature(i, j);
                    auto nouvelle_temp = std::max(chaleur(i, j), (
                        temperature(i - 1, j) +
                        temperature(i, j - 1) +
                        temperature(i, j + 1) +
                        temperature(i + 1, j) ) / 4 + BRUIT);

                    ctc(i, j).temperature = conduct * nouvelle_temp +
                        (1.0 - conduct) * ancienne_temp;
                    delta_temp += std::abs(
                        ancienne_temp - ctc(i, j).temperature);
                }
            }
        }

        return delta_temp / (larg * haut);
    }

private:
    std::size_t larg;
    std::size_t haut;
};


/**
 * Programme principal
 */
int main(int argc, char** argv)
{
    LePNG png;
    ModeleCTC carte_gpu;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " fichier.png" << std::endl;
        return 1;
    }

    try {
        // Charger l'image
        std::string nom_fichier(argv[1]);
        png.charger(nom_fichier);

        // Tranformer les pixels RGB en triplets CTC
        carte_gpu.redimensionner(png.largeur(), png.hauteur());
        std::transform(png.cbegin(), png.cend(), carte_gpu.begin(),
            [](const png_color & pixel) {
                return CTC {
                    (ctc_t)pixel.red,
                    (ctc_t)pixel.green,
                    (ctc_t)pixel.blue / 256
                };
            });
    }
    catch (const std::string message) {
        std::cerr << "Erreur: " << message << std::endl;
        return 2;
    }

    // Boucle principale
    ctc_t delta_temp = SEUIL_CONVERGENCE + 1.;
    unsigned int nb_iter = 0;

    while (delta_temp > SEUIL_CONVERGENCE && nb_iter < NB_MAX_ITER) {
        delta_temp = carte_gpu.un_pas_de_temps();
        nb_iter++;
    }

    const auto minmax = std::minmax_element(
        carte_gpu.cbegin(), carte_gpu.cend(),
        [](const CTC & a, const CTC & b) {
            return a.temperature < b.temperature;
        });
    std::cout << "Itération #" << nb_iter
        << ", ajustement moyen = " << delta_temp * 256 << " / 256"
        << ", t_min = " << minmax.first->temperature
        << ", t_max = " << minmax.second->temperature
        << std::endl;

    // Tranformer les triplets CTC en pixels RGB
    std::transform(carte_gpu.cbegin(), carte_gpu.cend(), png.begin(),
        [](const CTC & ctc) {
            return png_color {
                (png_byte)ctc.chaleur,
                (png_byte)ctc.temperature,
                (png_byte)(ctc.conduction * 256)
            };
        });

    // Aperçu après simulation
    std::cout << png << std::endl;

    try {
        png.enregistrer("resultat.png");
    }
    catch (const std::string message) {
        std::cerr << "Erreur: " << message << std::endl;
        return 3;
    }

    return 0;
}

