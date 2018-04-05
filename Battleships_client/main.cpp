#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <iomanip>

#include "common/enums.h"

#define MYPORT 32000
#define MAXDATASIZE 200

int boardSize;

void joinGame(int socket);
void initGame(int socket);

void initBoard(int socket, BoardElements ** board);
bool fillBoard(BoardElements ** board, const std::string &places);
bool getCoordinate(const std::string &places, unsigned long & lastIndex, int & x, int & y);

bool startGame(int socket);
bool runGame(int socket, BoardElements **board, BoardElements **adversaryBoard, bool playing);
void updateBoard(MsgTypes type, BoardElements ** board, const std::string &message, unsigned long nextDelimiter);

bool getFireCoordinate(const std::string &message, unsigned long nextDelimiter, int &x, int &y);
bool sinkedBoat(int x, int y, int addX, int addY, BoardElements ** board);
bool sinkedBoat(int x, int y, BoardElements ** board);

int getMessageID(const std::string &message, unsigned long &nextParameterPos);
void display(BoardElements ** board);
void display(BoardElements ** board, BoardElements ** adversaryBoard);


int main() {

    // region socket configuration

    int sockfd;

    struct sockaddr_in sock_addr{};
    struct in_addr serv_addr{};

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Creation socket : ");
        exit(EXIT_FAILURE);
    }

    char inputIP[15] = "127.0.0.1";

    if ((serv_addr.s_addr = inet_addr(inputIP)) == INADDR_NONE)
    {
        fprintf(stderr, "Adresse IP invalide \n");
        exit(EXIT_FAILURE);
    }

    // Configuration Obligatoire

    sock_addr.sin_addr = serv_addr;
    sock_addr.sin_port = htons(MYPORT);
    sock_addr.sin_family = AF_INET;
    bzero(&(sock_addr.sin_zero), 8);

    // endregion

    // region Game Initialisation

    if (connect(sockfd, (struct sockaddr*) &sock_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Connect");
        exit(EXIT_FAILURE);
    }

    joinGame(sockfd);

    // endregion

    // region init Board

    auto ** board = new BoardElements*[boardSize];
    auto ** adversaryBoard = new BoardElements*[boardSize];

    for (int i = 0; i < boardSize; i++)
    {
        board[i] = new BoardElements[boardSize];
        adversaryBoard[i] = new BoardElements[boardSize];
        for (int j = 0; j < boardSize; j++)
        {
            board[i][j] = EMPTY;
            adversaryBoard[i][j] = EMPTY;
        }
    }

    initBoard(sockfd, board);

    // endregion

    // region Game Running

    bool winner;
    if (startGame(sockfd))
    {
        std::cout << "c'est à vous de commencer !" << std::endl;
        winner = runGame(sockfd, board, adversaryBoard, true);
    }
    else
    {
        std::cout << "c'est à votre adversaire de commencer !" << std::endl;
        winner = runGame(sockfd, board, adversaryBoard, false);
    }

    if (winner)
    {
        std::cout << "Vous avez Gagné !" << std::endl;
    }
    else
    {
        std::cout << "Vous avez Perdu !" << std::endl;
    }

    // endregion

    // region Desalloc

    for (int i = 0; i < boardSize; i++)
    {
        delete board[i];
        delete adversaryBoard[i];
    }
    delete[] board;
    delete[] adversaryBoard;

    // endregion

    return 0;
}

void joinGame(int socket)
{
    std::cout << "Chargement..." << std::endl;
    char message[MAXDATASIZE];

    while (true) {
        bzero(message, MAXDATASIZE);
        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        std::string string(message);
        unsigned long nextDelimiter;
        int id;
        std::string temp;

        if ((id = getMessageID(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case ASK_INIT_GAME:
                initGame(socket);
                std::cout << "En attente du deuxième joueur..." << std::endl;
                break;

            case JOIN_SUCCESS:
                temp = string.substr(nextDelimiter, string.find("||", nextDelimiter) - nextDelimiter);
                std::cout << "La partie va bientot commencé ! La taille du plateau est de "
                          << temp << " par " << temp << std::endl << std::endl;
                std::cout << std::endl << "==============================================================" << std::endl;
                boardSize = stoi(temp);
                return;

            default:
                continue;
        }
    }
}
void initGame(int socket)
{
    std::cout << "Veuillez entrer la taille du plateau (entre 3 et 20) :" << std::endl;
    char message[MAXDATASIZE];

    while (true) {
        char inputSize[2];
        std::cin >> inputSize;

        sprintf(message, "1100|%s||", inputSize);

        if (send(socket, message, sizeof(message), 0) == -1){
            perror("send message : ");
            exit(EXIT_FAILURE);
        }

        bzero(message, MAXDATASIZE);

        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        std::string string(message);
        unsigned long nextDelimiter;
        int id;

        if ((id = getMessageID(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case BOARD_TOO_SMALL:
            case BOARD_TOO_BIG:
                std::cout << "La grille doit faire entre 3 et 20 cases..." << std::endl;
                break;

            case INIT_SUCCESS:
                std::cout << "La grille à bien été initialisé..." << std::endl;
                return;

            default:
                std::cout << "Veuillez entrer la taille du plateau (entre 3 et 20) :" << std::endl;
                continue;
        }
    }
}

void initBoard(int socket, BoardElements ** board)
{
    std::cout << "Veuillez rentrer les coordonnées de vos bateaux : " << std::endl
              << "(Exemple : A1,A2,B3,C3)" << std::endl
              << "il doit y avoir moins de 20% de bateaux" << std::endl;
    char message[MAXDATASIZE];
    char configuration[MAXDATASIZE - 20];

    while (true) {
        display(board);

        bzero(message, MAXDATASIZE);
        std::cout << "Entrez votre configuration : ";
        std::cout.flush();
        std::cin >> configuration;

        sprintf(message, "%d|%s||", INIT_GRID, configuration);

        if (send(socket, message, sizeof(message), 0) == -1){
            perror("send message : ");
            exit(EXIT_FAILURE);
        }

        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        std::string string(message);
        unsigned long nextDelimiter;
        int id;
        std::string temp;

        if ((id = getMessageID(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case ERR_BOAT_QTY:
                std::cout << "Il ne doit pas y avoir plus de 20% de la grille occupé par des bateaux soit : "
                          << boardSize * boardSize * 0.2 << " cases." << std::endl;
                break;

            case ERR_BOAT_OUTSIDE:
                std::cout << "Les coordonnées ne sont pas valide." << std::endl;
                break;

            case GRID_SUCCESS:
                fillBoard(board, configuration);
                display(board);
                std::cout << "en Attente du deuxième joueur..." << std::endl;
                return;

            default:
                continue;
        }
    }
}
bool fillBoard(BoardElements ** board, const std::string &places)
{
    unsigned long lastIndex = 0;
    std::string temp;
    int x,y;

    while (true)
    {
        if (getCoordinate(places, lastIndex, x, y))
        {
            if (x >= boardSize || x < 0 || y >= boardSize || y < 0)
                return false;

            board[x][y] = BOAT;
        }
        else
        {
            return false;
        }
        if (lastIndex == 0) return true;
    }
}
bool getCoordinate(const std::string &places, unsigned long & lastIndex, int & x, int & y)
{
    std::string id_string;
    unsigned long temp = lastIndex;

    try {
        lastIndex = places.find_first_of(',', lastIndex);
        id_string = places.substr(temp, lastIndex++ - temp);
        x = (int)id_string[0] - 65;
        y = stoi(id_string.substr(1, id_string.length())) - 1;

        return true;
    }
    catch (std::exception &e) {
        return false;
    }
}

bool startGame(int socket)
{
    char message[MAXDATASIZE];

    while (true) {
        bzero(message, MAXDATASIZE);

        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        std::string string(message);
        unsigned long nextDelimiter;
        int id;
        std::string temp;

        if ((id = getMessageID(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case START:
                temp = string.substr(nextDelimiter, string.find("||", nextDelimiter) - nextDelimiter);
                return temp == "true";

            default:
                continue;
        }
    }
}

bool runGame(int socket, BoardElements **board, BoardElements **adversaryBoard, bool playing)
{
    char message[MAXDATASIZE];
    char configuration[3];
    BoardElements  ** currentBoard;

    display(board, adversaryBoard);
    while (true) {
        bzero(message, MAXDATASIZE);

        if (playing)
        {
            std::cout << "Entrez les coordonnées de tir :" << std::endl;
            std::cin >> configuration;

            sprintf(message, "%d|%s||", FIRE, configuration);

            if (send(socket, message, sizeof(message), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            bzero(message, MAXDATASIZE);
        }

        currentBoard = (playing) ? adversaryBoard : board;
        playing = false;

        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        std::string string(message);
        unsigned long nextDelimiter;
        int id;
        std::string temp;

        if ((id = getMessageID(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case PLAY:
                std::cout << "A vous de jouer !" << std::endl;
                playing = true;
                continue;

            case ERR_ALREADY_FIRED:
                std::cout << "Vous avez déjà ciblé cette case..." << std::endl;
                playing = true;
                continue;
            case ERR_COORDINATE:
                std::cout << "Coordonnées non valide..." << std::endl;
                playing = true;
                continue;

            case HIT:
            case SINK:
            case MISS:
                updateBoard(static_cast<MsgTypes>(id), currentBoard, message, nextDelimiter);
                display(board, adversaryBoard);
                continue;

            case WIN:
            case LOOSE:
                updateBoard(SINK, currentBoard, message, nextDelimiter);
                display(board, adversaryBoard);
                return (id == WIN);

            default:
                continue;
        }
    }
}
void updateBoard(MsgTypes type, BoardElements ** board, const std::string &message, unsigned long nextDelimiter)
{
    int x, y;
    if (!getFireCoordinate(message, nextDelimiter, x, y)) return;

    switch (type)
    {
        case HIT:
            std::cout << "Touché !" << std::endl;
            board[x][y] = HITTED;
            return;

        case MISS:
            std::cout << "Loupé !" << std::endl;
            board[x][y] = MISSED;
            return;

        case SINK:
            std::cout << "Coulé !" << std::endl;
            sinkedBoat(x, y, board);
            return;

        default:
            return;
    }
}
bool getFireCoordinate(const std::string &message, unsigned long nextDelimiter, int &x, int &y)
{
    try {
        unsigned long lastIndex = message.find_first_of('|', nextDelimiter);
        std::string id_string = message.substr(nextDelimiter, lastIndex - nextDelimiter);
        x = (int)id_string[0] - 65;
        y = stoi(id_string.substr(1, id_string.length())) - 1;
    }
    catch (std::exception &e)
    {
        return false;
    }

    return !(x >= boardSize || x < 0 || y >= boardSize || y < 0);

}

bool sinkedBoat(int x, int y, int addX, int addY, BoardElements ** board)
{
    if (x + addX < 0 || x + addX >= boardSize || y + addY < 0 && y + addY >= boardSize)
        return true;

    BoardElements element = board[x + addX][y + addY];

    if (element == BOAT)
        return false;
    if (element == HITTED)
    {
        board[x + addX][y + addY] = SINKED;
        return sinkedBoat(x + addX, y + addY, addX, addY, board);
    }

    return true;
}
bool sinkedBoat(int x, int y, BoardElements ** board)
{
    board[x][y] = SINKED;

    if ((x + 1 < boardSize  && board[x + 1][y] == BOAT) ||
        (x - 1 >= 0         && board[x - 1][y] == BOAT) ||
        (y + 1 < boardSize  && board[x][y + 1] == BOAT) ||
        (y - 1 >= 0         && board[x][y - 1] == BOAT))
    {
        return false;
    }
    else if ((x + 1 < boardSize && board[x + 1][y] == HITTED) ||
             (x - 1 >= 0        && board[x - 1][y] == HITTED))
    {
        return sinkedBoat(x, y, 1, 0, board) && sinkedBoat(x, y, -1, 0, board);
    }
    else if ((y + 1 < boardSize && board[x][y + 1] == HITTED) ||
             (y - 1 >= 0        && board[x][y - 1] == HITTED))
    {
        return sinkedBoat(x, y, 0, 1, board) && sinkedBoat(x, y, 0, -1, board);
    }

}

/* utilities */

int getMessageID(const std::string &message, unsigned long &nextParameterPos)
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

char getGraphicRepresentationOf(BoardElements element)
{
    switch (element)
    {
        case EMPTY:
            return ' ';
        case BOAT:
            return 'B';
        case MISSED:
            return 'O';
        case HITTED:
            return 'x';
        case SINKED:
            return '#';
    }
}
void display(BoardElements ** board)
{
    std::cout << std::endl << "  ";
    for (int j = 0; j < boardSize; j++)
    {
        std::cout << j + 1 << " ";
    }
    std::cout << std::endl;

    for (int i = 0; i < boardSize; i++)
    {
        std::cout << (char)(i+65) << "|";

        for (int j = 0; j < boardSize; j++)
        {
            std::cout << getGraphicRepresentationOf(board[i][j]) << ' ';
        }

        std::cout << std::endl;
    }
    std::cout << std::endl;
}
void display(BoardElements ** board, BoardElements ** adversaryBoard)
{
    std::cout << std::endl << std::setw(boardSize + 5) << "Votre Plateau" << "   "
              << "Plateau de l'adversaire" << std::endl;

    std::cout << "  ";
    for (int j = 0; j < boardSize; j++)
    {
        std::cout << j + 1 << " ";
    }
    std::cout << "     ";
    for (int j = 0; j < boardSize; j++)
    {
        std::cout << j + 1 << " ";
    }
    std::cout << std::endl;

    for (int i = 0; i < boardSize; i++)
    {
        std::cout << (char)(i+65) << "|";

        for (int j = 0; j < boardSize; j++)
        {
            std::cout << getGraphicRepresentationOf(board[i][j]) << ' ';
        }

        std::cout << "   " << (char)(i+65) << "|";

        for (int j = 0; j < boardSize; j++)
        {
            std::cout << getGraphicRepresentationOf(adversaryBoard[i][j]) << ' ';
        }

        std::cout << std::endl;
    }
    std::cout << std::endl;
}
