#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <iomanip>

#include "enums.h"

#define MYPORT 32000
#define MAXDATASIZE 200

int taillePlateau;

void rejoindreJeu(int socket);
void creerPlateau(int socket);

void initPlateauJoueur(int socket, StatutCase ** board);
bool remplirPlateau(StatutCase ** board, const std::string &places);
bool recupererVerifierCoordonnées(const std::string &places, unsigned long & lastIndex, int & x, int & y);

bool demarrerJeu(int socket);
bool jouer(int socket, StatutCase **board, StatutCase **adversaryBoard, bool playing);
void metAJourBateau(MsgTypes type, StatutCase ** board, const std::string &message, unsigned long nextDelimiter);

bool couleBateau(int x, int y, int addX, int addY, StatutCase ** board);
bool couleBateau(int x, int y, StatutCase ** board);

int convertirCodeRetour(const std::string &message, unsigned long &nextParameterPos);
void afficher(StatutCase ** board);
void afficher(StatutCase ** board, StatutCase ** adversaryBoard);

using namespace std;


int main() {

    //Configuration des sockets

    int sockfd;
    struct sockaddr_in sock_addr{};
    struct in_addr serv_addr{};
	char inputIP[15] = "127.0.0.1";

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Creation socket : ");
        exit(EXIT_FAILURE);
    }

    if ((serv_addr.s_addr = inet_addr(inputIP)) == INADDR_NONE)
    {
        fprintf(stderr, "Adresse IP invalide \n");
        exit(EXIT_FAILURE);
    }

    sock_addr.sin_addr = serv_addr;
    sock_addr.sin_port = htons(MYPORT);
    sock_addr.sin_family = AF_INET;
    bzero(&(sock_addr.sin_zero), 8);
	
    if (connect(sockfd, (struct sockaddr*) &sock_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Connect error");
        exit(EXIT_FAILURE);
    }

    rejoindreJeu(sockfd); //On attent qu'un deuxième joueur se connecte

	StatutCase** plateau = creerPlateauJoueur();
	StatutCase** plateauAdverse = creerPlateauJoueur();

	initPlateauJoueur(sockfd, plateau); //Demande au joueur de placer ses bateaux sur sa grille

    bool vainqueur;
	bool joueEnPremier = demarrerJeu(sockfd); // On recoit la réponse du serveur qui démarre le jeu 
											  // et définit le premier joueur

    if (joueEnPremier)
        cout << "Vous commencez. " << endl;
       
    else
        cout << "Votre adversaire commence !" << endl;

	vainqueur = jouer(sockfd, plateau, plateauAdverse, joueEnPremier); //Ici se déroule le jeu

    if (vainqueur)
        cout << "Vous avez gagné" << endl;
    
    else
        cout << "Vous avez perdu" << endl;
    

	detruirePlateau();
   
    return 0;
}

void rejoindreJeu(int socket)
{
   
    char charMessage[MAXDATASIZE];

    while (true) 
	{
        bzero(charMessage, MAXDATASIZE);
        if (recv(socket, &charMessage, MAXDATASIZE, MSG_WAITALL) == -1)
		{
            perror("Receive error");
            exit(EXIT_FAILURE);
        }

        string stringMessage = charMessage;
        unsigned long nextDelimiter;
        int code;
        string tailleString;

        if ((code = convertirCodeRetour(stringMessage, nextDelimiter)) == -1) 
            continue;

        switch (code) {
            case CREER_JEU:
                creerPlateau(socket);
                cout << "Attente du deuxième joueur..." << endl;
                break;

            case JOIN_SUCCESS:
				tailleString = stringMessage.substr(nextDelimiter, stringMessage.find("||", nextDelimiter) - nextDelimiter);
                taillePlateau = stoi(tailleString);
				cout << "La partie va commencer \n Taille du plateau : " << taillePlateau << "x" << taillePlateau << endl << endl;
                return;

            default:
                continue;
        }
    }
}
void creerPlateau(int socket)
{
    cout << "Taille du plateau (entre 3 et 20) :" << endl;
    char charMessage[MAXDATASIZE];

    while (true) {
        char taille[2];
        cin >> taille;

        sprintf(messageCode, "1100|%s||", taille);

        if (send(socket, messageCode, sizeof(messageCode), 0) == -1){
            perror("Message envoi");
            exit(EXIT_FAILURE);
        }

        bzero(messageCode, MAXDATASIZE);

        if (recv(socket, &messageCode, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Reception");
            exit(EXIT_FAILURE);
        }

        string stringMessage(message);
        unsigned long nextDelimiter;
        int id;

        if ((id = convertirCodeRetour(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case BOARD_TOO_SMALL:
				cout << "Taille trop petite (<5)" << endl;
            case BOARD_TOO_BIG:
                cout << "Taille trop grande (>30)" << endl;
                break;

            case INIT_SUCCESS:
                cout << "Grille créée" << endl;
                return;

            default:
                cout << "Entrer la taille du plateau (entre 5 et 30) :" << endl;
                continue;
        }
    }
}


StatutCase** creerPlateauJoueur()
{
	StatutCase** plateau = new StatutCase*[taillePlateau];

	for (int i = 0; i < taillePlateau; i++)
	{
		plateau[i] = new StatutCase[taillePlateau];
		for (int j = 0; j < taillePlateau; j++)
			plateau[i][j] = VIDE;
	}

	return plateau
}

void initPlateauJoueur(int socket, StatutCase ** plateau)
{
	cout << "Il doit y avoir maximum 20% des cases occupées par un bateau soit : 20% * " 
		<< taillePlateau << "x" << taillePlateau << " : " << taillePlateau * taillePlateau * 0.2 << " cases.";
              
    char charMessage[MAXDATASIZE];
    char confi[MAXDATASIZE - 20];

    while (true) {
        afficher(plateau);

        bzero(charMessage, MAXDATASIZE);
		cout << "Entrez toutes les cases occupées par un bateau : (Exemple : A1,A2,B3,C3)" << endl;
		cout.flush();
        cin >> config;

        sprintf(charMessage, "%d|%s||", INIT_GRID, config);

        if (send(socket, charMessage, sizeof(charMessage), 0) == -1){
            perror("send message : ");
            exit(EXIT_FAILURE);
        }

        if (recv(socket, &charMessage, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        string codeRetourString(charMessage);
        unsigned long nextDelimiter;
        int codeRetour;

        if ((codeRetour = convertirCodeRetour(codeRetourString, nextDelimiter)) == -1)
            continue;

        switch (codeRetour) {
            case ERR_BOAT_QTY:
				cout << "Il doit y avoir maximum 20% des cases occupées par un bateau soit : 20% * "
					<< taillePlateau << "x" << taillePlateau << " = " << taillePlateau * taillePlateau * 0.2 << " cases.";
                break;

            case ERR_BOAT_OUTSIDE:
                cout << "Les coordonnées ne sont pas valides." << endl;
                break;

            case GRID_SUCCESS:
                remplirPlateau(plateau, config);
                afficher(plateau);
                cout << "Grille créée avec succès, en attente du deuxième joueur..." << endl;
                return;

            default:
                continue;
        }
    }
}


bool remplirPlateau(StatutCase ** plateau, const string &config)
{
    int lastIndex = 0;
    string temp;
    int x,y;
	bool coordonneesValides;
	string id_string;
	int temp = lastIndex;

    for(;;)
    {
		try {
			lastIndex = places.find_first_of(',', lastIndex);
			id_string = places.substr(temp, (lastIndex++) - temp);
			x = (int)id_string[0] - 65; //Récupère la lettre d'une coordonnée puis la transforme en chiffre
			y = stoi(id_string.substr(1, id_string.length())) - 1; //Récupère le chiffre d'une coordonnée
			coordonneesValides = (x <= taillePlateau && x > 0 && y <= taillePlateau && y > 0); //On test si les coordonnées sont trop grandes ou trop petites
		}
		catch (exception &e) {
			coordonneesValides = false;
		}

        if (coordonneesValides)
            board[x][y] = OCCUPEE;
        
        else
            return false;
        
        if (lastIndex == 0) return true; //On a placé toutes les coordonnées, on sort de la boucle
    }
}

bool demarrerJeu(int socket)
{
    char charMessage[MAXDATASIZE];

    while (true) {
        bzero(charMessage, MAXDATASIZE);

        if (recv(socket, &charMessage, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        string string(charMessage);
        unsigned long nextDelimiter;
        int codeRetour;
		string joueEnPremier;

        if ((codeRetour = convertirCodeRetour(string, nextDelimiter)) == -1)
            continue;

        switch (codeRetour) {
            case START:
                joueEnPremier = string.substr(nextDelimiter, string.find("||", nextDelimiter) - nextDelimiter);
                return joueEnPremier == "true";

            default:
                continue;
        }
    }
}

bool jouer(int socket, StatutCase **plateau, StatutCase **plateauAdverse, bool playing)
{
    char message[MAXDATASIZE];
    char configuration[3];
    StatutCase  ** plateauActuel;

    afficher(plateau, plateauAdverse);

    while (true) {
        bzero(message, MAXDATASIZE);
		bzero(configuration, 3);

        if (playing)
        {
            cout << "Entrez la case sur laquelle vous voulez tirer :" << std::endl;
            cin >> configuration;

            sprintf(message, "%d|%s||", FIRE, configuration); 

            if (send(socket, message, sizeof(message), 0) == -1){ //Envoie de la case choisie
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            bzero(message, MAXDATASIZE); //Réinitialisation du buffer
        }

		plateauActuel = (playing) ? plateauAdverse : plateau;
        playing = false;

        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) { //On recoit la réponse du serveur
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        string string(message);
        unsigned long nextDelimiter;
        int codeRetour;
        string temp;

        if ((codeRetour = convertirCodeRetour(string, nextDelimiter)) == -1) //Récupère et vérifie le code de retour envoyé par le serveur
            continue;

        switch (codeRetour) {
            case PLAY: 
                cout << "C'est a votre tour." << std::endl;
                playing = true;
                continue;

            case ERR_ALREADY_FIRED:
                cout << "Vous avez déjà tiré sur cette case." << std::endl;
                playing = true;
                continue;
            case ERR_COORDINATE:
                cout << "Coordonnées non valides." << std::endl;
                playing = true;
                continue;

            case M_TOUCHE:
            case M_COULE:
            case M_MANQUE:
                metAJourBateau(static_cast<MsgTypes>(codeRetour), plateauActuel, message, nextDelimiter);
                afficher(plateau, plateauAdverse);
                continue;

            case WIN:
            case LOOSE:
                metAJourBateau(COULE, plateauActuel, message, nextDelimiter);
                afficher(plateau, plateauAdverse);
                return (codeRetour == WIN); 

            default:
                continue;
        }
    }
}
void metAJourBateau(MsgTypes type, StatutCase ** board, const std::string &message, unsigned long nextDelimiter)
{
    int x, y;

	try { //vérifie et récupère les coordonnées envoyées par le serveur 
		unsigned long lastIndex = message.find_first_of('|', nextDelimiter);
		std::string id_string = message.substr(nextDelimiter, lastIndex - nextDelimiter);
		x = (int)id_string[0] - 65;
		y = stoi(id_string.substr(1, id_string.length())) - 1;

		if (x >= taillePlateau || x < 0 || y >= taillePlateau || y < 0)
			return;
	}
	catch (std::exception &e)
		return;

	

    switch (type)
    {
        case M_TOUCHE:
            cout << "Touché !" << endl;
            board[x][y] = TOUCHE;
            return;

        case M_MANQUE:
            cout << "Manqué !" << endl;
            board[x][y] = MANQUE;
            return;

        case M_COULE:
            cout << "Coulé !" << endl;
            couleBateau(x, y, board);
            return;

        default:
            return;
    }
}



bool couleBateau(int x, int y, int addX, int addY, StatutCase ** board)
{
    if (x + addX < 0 || x + addX >= taillePlateau || y + addY < 0 && y + addY >= taillePlateau)
        return true;

    StatutCase element = board[x + addX][y + addY];

    if (element == OCCUPEE)
        return false;
    if (element == TOUCHE)
    {
        board[x + addX][y + addY] = COULE;
        return couleBateau(x + addX, y + addY, addX, addY, board);
    }

    return true;
}



bool couleBateau(int x, int y, StatutCase ** plateau)
{
    board[x][y] = COULE;

    if ((x + 1 < taillePlateau  && plateau[x + 1][y] == OCCUPEE) || (x - 1 >= 0 && plateau[x - 1][y] == OCCUPEE) ||
        (y + 1 < taillePlateau  && plateau[x][y + 1] == OCCUPEE) || (y - 1 >= 0 && plateau[x][y - 1] == OCCUPEE))
        return false;
    
    else if ((x + 1 < taillePlateau && plateau[x + 1][y] == TOUCHE) || (x - 1 >= 0 && plateau[x - 1][y] == TOUCHE))
        return couleBateau(x, y, 1, 0, plateau) && couleBateau(x, y, -1, 0, plateau);
    
    else if ((y + 1 < taillePlateau && plateau[x][y + 1] == TOUCHE) || (y - 1 >= 0 && plateau[x][y - 1] == TOUCHE))
        return couleBateau(x, y, 0, 1, plateau) && couleBateau(x, y, 0, -1, plateau);
    

}

int convertirCodeRetour(const std::string &message, unsigned long &nextParameterPos)
{
    std::string id_string;

    try {
        nextParameterPos = message.find_first_of('|', 0);
        id_string = message.substr(0, nextParameterPos++);
        return stoi(id_string);
    }
    catch (std::exception &e) {
        return -1;
    }
}

char afficheCase(BoardElements element)
{
    switch (element)
    {
        case VIDE:
            return ' ';
        case OCCUPEE:
            return 'O';
        case MANQUE:
            return 'M';
        case TOUCHE:
            return 'T';
        case COULE:
            return '#';
    }
}
void afficher(StatutCase ** plateau)
{
    cout << endl << "  ";
    for (int i = 0; i < taillePlateau; i++)
        cout << i + 1 << " ";

    cout << endl << endl;

    for (int i = 0; i < taillePlateau; i++)
    {
        cout << (char)(i+65) << "|";

        for (int j = 0; j < taillePlateau; j++)
            std::cout << afficheCase(plateau[i][j]) << " ";

        cout << endl;
    }
    cout << endl << endl;
}
void afficher(StatutCase ** plateau, StatutCase ** plateauAdverse)
{
    cout <<endl << setw(taillePlateau + 5) << "Votre plateau" << "     " << "Plateau de l'adversaire" << endl << " ";

	for (int i = 0; i < taillePlateau; i++) //Première ligne de chiffre pour le premier plateau
        cout << i + 1 << " ";
    
    cout << "     ";
    for (int i = 0; i < taillePlateau; i++) //Première ligne de chiffre pour le deuxième plateau
        cout << i + 1 << " ";
    
    cout << endl;

    for (int i = 0; i < taillePlateau; i++) //Affichage ligne par ligne des deux plateaux à coté
    {
        cout << (char)(i+65) << "|";

        for (int j = 0; j < taillePlateau; j++) //Plateau du joueur
            cout << afficheCase(plateau[i][j]) << ' ';
        

        cout << "   " << (char)(i+65) << "|";

        for (int j = 0; j < taillePlateau; j++) //Plateau adversaire
            cout << afficheCase(plateauAdverse[i][j]) << ' ';
        

        cout << endl;
    }
    cout << endl;
}

void detruirePlateau()
{
	for (int i = 0; i < taillePlateau; i++)
	{
		delete plateau[i];
		delete plateauAdverse[i];
	}
	delete[] plateau;
	delete[] plateauAdverse;
}
