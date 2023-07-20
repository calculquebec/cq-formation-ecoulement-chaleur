#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <png.h>


/**
 * Classe facilitant la lecture-écriture (Le) de fichiers PNG
 */
class LePNG
{
public:
    LePNG() {
        memset(&entete, 0, sizeof entete);
        entete.version = PNG_IMAGE_VERSION;
    }

    ~LePNG() {
        png_image_free(&entete);
    }

    /**
     * Chargement des métadonnées du fichier PNG
     */
    void initLecture(const std::string & nom_fichier) {
        if (!png_image_begin_read_from_file(&entete, nom_fichier.c_str()))
            throw nom_fichier + " - " + entete.message;
    }

    /**
     * Lecture des pixels RGB (rouge, vert, bleu), 3x8 bits
     */
    void lireRGB(png_bytep data) {
        if (data == NULL)
            throw "data buffer unallocated";

        entete.format = PNG_FORMAT_RGB;

        if (!png_image_finish_read(&entete, NULL, data, 0, NULL))
            throw entete.message;
    }

    inline png_uint_32 largeur() const { return entete.width; }
    inline png_uint_32 hauteur() const { return entete.height; }

private:
    png_image entete;
};


/**
 * Triplet (Chaleur, Température, Conduction), 8 bits par composante
 */
typedef struct {
    png_byte chal;  // Intensité de la source de chaleur
    png_byte temp;  // En degrés Celsius
    png_byte cond;  // Facteur de conduction de chaleur
} CTC;


/**
 * Modèle de grille 2D de valeurs de chaleur, température et conduction
 */
class ModeleCTC: public std::vector<CTC>
{
public:
    ModeleCTC(): larg(0), haut(0) {}

    /**
     * Redimensionner la grille et lire les données du fichier PNG
     */
    void initialiser(LePNG & png) {
        larg = png.largeur();
        haut = png.hauteur();

        resize(larg * haut);
        png.lireRGB((png_bytep)data());
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
    inline png_byte chaleur(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne).chal;
    }
    inline png_byte temperat(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne).temp;
    }
    inline png_byte conduction(std::size_t rangee, std::size_t colonne) const {
        return at(rangee * larg + colonne).cond;
    }

    inline std::size_t largeur() const { return larg; }
    inline std::size_t hauteur() const { return haut; }

private:
    std::size_t larg;
    std::size_t haut;
};


/**
 * Affiche un aperçu du modèle en ASCII
 * @param ostr   Objet std::ostream tel que std::cout
 * @param modele Le modèle chaleur-température-conduction
 * @return L'objet std::ostream pour des appels à la chaîne
 */
std::ostream& operator<<(std::ostream & ostr, const ModeleCTC & modele)
{
    const char ascii[] = " .,!~:;+=xzXZ%#@";
    const auto largeur = modele.largeur(), hauteur = modele.hauteur();
    const auto sautH = modele.largeur() / 80;  // caractères de large
    const auto sautV = sautH * 2;

    ostr << "Taille du modèle: " << largeur << " x " << hauteur << std::endl;

    // Pour chaque bloc complet de taille sautH x sautV
    for (auto i = sautV + hauteur % sautV / 2; i < hauteur; i += sautV) {
        for (auto j = sautH + largeur % sautH / 2; j < largeur; j += sautH) {
            unsigned int cond_tot = 0;

            // Calculer la moyenne des valeurs de conduction ( [0, 256[ )
            for (auto ii = i - sautV; ii < i; ++ii) {
                for (auto jj = j - sautH; jj < j; ++jj) {
                    cond_tot += modele.conduction(ii, jj);
                }
            }

            // Sélectionner un des 16 caractères ASCII (/16 = *16/256)
            ostr << ascii[cond_tot / (sautV * sautH * 16)];
        }

        ostr << std::endl;
    }

    return ostr;
}


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
        png.initLecture(argv[1]);
        carte_gpu.initialiser(png);
        std::cout << carte_gpu << std::endl;
    }
    catch (const std::string message) {
        std::cerr << "Erreur: " << message << std::endl;
        return 2;
    }

    return 0;
}

