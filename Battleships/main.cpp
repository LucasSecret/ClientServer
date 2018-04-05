#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>

#include "common/enums.h"

using namespace std;

#define MYPORT 32000
#define BACKLOG 10
#define MAXDATASIZE 200


int boardSize;
string entete = "SERVER>> ";

int getMessageID(const string &msg, unsigned long &nextParameterPos)
{
    string id_string;

    try {
        nextParameterPos = msg.find_first_of('|', 0);
        id_string = msg.substr(0, nextParameterPos++);
        return stoi(id_string);
    }
    catch (exception &e) {
        return -1;
    }
}

void initGame(int socket)
{
    char msg[MAXDATASIZE];

    while (true) {
        bzero(msg, MAXDATASIZE);
        if (recv(socket, &msg, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Erreur à la réception");
            exit(EXIT_FAILURE);
        }

        string chaine(msg);
        unsigned long nextCom;
        int id;
	string tmp;

        if ((id = getMessageID(chaine, nextCom)) == -1)
            continue;

        switch (id) {
            case INIT_GAME:
                tmp = chaine.substr(nextCom, chaine.find('|', nextCom) - nextCom);
                break;

            default:
                cout << "Erreur message : " << msg << endl;
                cout.flush();
                continue;
        }

        try {
            boardSize = stoi(tmp);
        }
        catch (exception &e) {
            if (send(socket, "1107||", sizeof("1107||"), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (boardSize < 3)
        {
            if (send(socket, "1107||", sizeof("1107||"), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }
        else if (boardSize > 20)
        {
            if (send(socket, "1108||", sizeof("1108||"), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        sprintf(msg, "%d||", INIT_SUCCESS);
	
        if (send(socket, msg, sizeof(msg), 0) == -1){
            perror("Envoie du message : ");
            exit(EXIT_FAILURE);
        }

        return;
    }
}

bool getCoordinate(const string &places, unsigned long & lastIndex, int & x, int & y)
{
    string id_string;
    unsigned long tmp = lastIndex;

    try {
        lastIndex = places.find_first_of(',', lastIndex);
        id_string = places.substr(tmp, lastIndex++ - tmp);
        x = (int)id_string[0] - 65;
        y = stoi(id_string.substr(1, id_string.length())) - 1;

        return true;
    }
    catch (exception &e) {
        return false;
    }
}

bool checkBoardValidity(int socket, BoardElements ** board)
{
    int boatQty = 0;

    for (int i = 0; i < boardSize; i++)
        for (int j = 0; j < boardSize; j++)
            if(board[i][j] == BOAT)
                boatQty++;

    if (boatQty > (boardSize * boardSize) * 0.2)
    {
        if (send(socket, "1146||", sizeof("1146||"), 0) == -1){
            perror("Envoie du message : ");
            exit(EXIT_FAILURE);
        }
        return false;
    }

    return true;
}

bool fillBoard(BoardElements ** board, const string &places)
{
    unsigned long lastIndex = 0;
    string tmp;
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

void initBoard(int socket, BoardElements ** board)
{
    char msg[MAXDATASIZE];

    while (true) {
        bzero(msg, MAXDATASIZE);
        if (recv(socket, &msg, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Erreur de réception : ");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < boardSize; i++)
            for (int j = 0; j < boardSize; j++)
                board[i][j] = EMPTY;

        string chaine(msg);
        unsigned long nextCom;
        int id;
        string tmp;

        if ((id = getMessageID(chaine, nextCom)) == -1)
            continue;

        switch (id) {
            case INIT_GRID:
                tmp = chaine.substr(nextCom, chaine.find('|', nextCom) - nextCom);
                break;

            default:
                cout << "Erreur dans la syntaxe : " << msg << endl;
                cout.flush();
                continue;
        }

        if (!fillBoard(board, tmp))
        {
            if (send(socket, "1148||", sizeof("1148||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (checkBoardValidity(socket, board))
        {
            if (send(socket, "1145||", sizeof("1145||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            return;
        }
    }
}

bool getAttackTarget(const string &msg, unsigned long nextCom, int &x, int &y)
{
    try {
        unsigned long lastIndex = msg.find_first_of('|', nextCom);
        string id_string = msg.substr(nextCom, lastIndex - nextCom);
        x = (int)id_string[0] - 65;
        y = stoi(id_string.substr(1, id_string.length())) - 1;
    }
    catch (exception &e)
    {
        return false;
    }

    return !(x >= boardSize || x < 0 || y >= boardSize || y < 0);

}

bool boatSink(int x, int y, int addX, int addY, BoardElements ** adversaryBoard)
{
    if (x + addX < 0 || x + addX >= boardSize || y + addY < 0 && y + addY >= boardSize)
        return true;

    BoardElements element = adversaryBoard[x + addX][y + addY];

    if (element == BOAT)
        return false;
    if (element == HITTED)
        return boatSink(x + addX, y + addY, addX, addY, adversaryBoard);

    return true;
}

bool boatSinked(int x, int y, BoardElements ** adversaryBoard)
{
    if ((x + 1 < boardSize  && adversaryBoard[x + 1][y] == BOAT) ||
        (x - 1 >= 0         && adversaryBoard[x - 1][y] == BOAT) ||
        (y + 1 < boardSize  && adversaryBoard[x][y + 1] == BOAT) ||
        (y - 1 >= 0         && adversaryBoard[x][y - 1] == BOAT))
    {
        return false;
    }
    else if ((x + 1 < boardSize && adversaryBoard[x + 1][y] == HITTED) ||
             (x - 1 >= 0        && adversaryBoard[x - 1][y] == HITTED))
    {
        return boatSink(x, y, 1, 0, adversaryBoard) && boatSink(x, y, -1, 0, adversaryBoard);
    }
    else if ((y + 1 < boardSize && adversaryBoard[x][y + 1] == HITTED) ||
             (y - 1 >= 0        && adversaryBoard[x][y - 1] == HITTED))
    {
        return boatSink(x, y, 0, 1, adversaryBoard) && boatSink(x, y, 0, -1, adversaryBoard);
    }

}

bool boardContainsBoat(BoardElements ** adversaryBoard)
{
    for (int i = 0; i < boardSize; i++)
        for (int j = 0; j < boardSize; j++)
            if (adversaryBoard[i][j] == BOAT)
                return true;

    return false;
}

MsgTypes play(int socket, BoardElements ** adversaryBoard, int &x, int &y)
{
    char msg[MAXDATASIZE];

    while (true)
    {
        bzero(msg, MAXDATASIZE);
        if (recv(socket, &msg, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Erreur à la réception");
            exit(EXIT_FAILURE);
        }

        string chaine(msg);
        unsigned long nextCom;
        int id;
        string tmp;

        if ((id = getMessageID(chaine, nextCom)) == -1)
            continue;

        switch (id) {
            case FIRE:
                if (!getAttackTarget(msg, nextCom, x, y))
                {
                    if (send(socket, "1506||", sizeof("1506||"), 0) == -1){
                        perror("Envoie du message : ");
                        exit(EXIT_FAILURE);
                    }
                    continue;
                }
                break;

            default:
                cout << "Message invalide " << msg << endl;
                cout.flush();
                continue;
        }

        if (adversaryBoard[x][y] == MISSED || adversaryBoard[x][y] == HITTED || adversaryBoard[x][y] == SINKED)
        {
            if (send(socket, "1502||", sizeof("1502||"), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        string result;

        if (adversaryBoard[x][y] == EMPTY)
        {
            return MISS;
        }
        else
        {
            adversaryBoard[x][y] = HITTED;

            if (boatSinked(x, y, adversaryBoard))
            {
                if (!boardContainsBoat(adversaryBoard))
                {
                    return WIN;
                }
                else
                {
                    return SINK;
                }
            }
            else
            {
                return HIT;
            }
        }
    }
}


int main()
{
    cout<<"--> Bienvenu dans BoatCrusher alpha 0.0.01 <--"<<endl<<endl;
    srand(static_cast<unsigned int>(time(nullptr)));

    //region Socket Initialisation
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Creation socket : ");
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("Options socket : ");
        exit(EXIT_FAILURE);
    }
    else{
    	cout<<entete<<"L'initialisation c'est terminée correctement"<<endl;
    }

    // Configuration de l'adresse de transport
    struct sockaddr_in my_addr{};
    my_addr.sin_family = AF_INET;         // type de la socket
    my_addr.sin_port = htons(MYPORT);     // port, converti en reseau
    my_addr.sin_addr.s_addr = INADDR_ANY; // adresse, devrait être converti en reseau mais est egal à 0
    bzero(&(my_addr.sin_zero), 8);        // mise a zero du reste de la structure

    if((bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))) == -1)
    {
        perror("Creation point connexion : ");
        exit(EXIT_FAILURE);
    }
    else{
        cout<<entete<<"En attente du 1er client"<<endl;
    }
    //endregion

    //region First User Connection
    int p1_fd{};
    struct sockaddr_in addr{};
    socklen_t sin_size = sizeof(struct sockaddr);

    if(listen(sockfd, BACKLOG) == -1)
    {
        perror("Ecoute de la socket : ");
        exit(EXIT_FAILURE);
    }

    if((p1_fd = accept(sockfd, (struct sockaddr *)&addr, &sin_size)) == -1)
    {
        perror("Accept premier client : ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    else{
	cout<<entete<<"Le client 1 c'est connecté"<<endl;
    }

    if (send(p1_fd, "1050||", sizeof("1050||"), 0) == -1){
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }
    //endregion

    thread init_game(initGame, p1_fd);

    //region Second User Connection
    int p2_fd{};
    if((p2_fd = accept(sockfd, (struct sockaddr *)&addr, &sin_size)) == -1)
    {
        perror("Accept second client : ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    else{
	cout<<entete<<"Le client 2 c'est connecté"<<endl;
    }
    //endregion

    //region Init Game & Board
    init_game.join();

    char buf[20];
    sprintf(buf, "%d|%d||", JOIN_SUCCESS, boardSize);

    if (send(p1_fd, buf, sizeof(buf), 0) == -1){
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }
    if (send(p2_fd, buf, sizeof(buf), 0) == -1){
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }

    BoardElements** P1Board = new BoardElements*[boardSize];
    BoardElements** P2Board = new BoardElements*[boardSize];

    for (int i = 0; i < boardSize; i++)
    {
        P1Board[i] = new BoardElements[boardSize];
        P2Board[i] = new BoardElements[boardSize];
    }

    thread init_p1(initBoard, p1_fd, P1Board);
    thread init_p2(initBoard, p2_fd, P2Board);

    init_p1.join();
    init_p2.join();

    // endregion

    // region Launch Game
    cout<<entete<<"Une partie à commencé"<<endl;

    char startMessageP1[11];
    char startMessageP2[11];
    bool isFirstPlayerTurn = (random() % 2 == 0);

    if (isFirstPlayerTurn)
    {
        strcpy(startMessageP1, "1500|true||");
        strcpy(startMessageP2, "1500|false||");
    }
    else
    {
        strcpy(startMessageP2, "1500|true||");
        strcpy(startMessageP1, "1500|false||");
    }

    if (send(p1_fd, startMessageP1, sizeof(startMessageP1), 0) == -1){
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }
    if (send(p2_fd, startMessageP2, sizeof(startMessageP2), 0) == -1){
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }

    // endregion

    // region Game Running
    bool gameRunning = true;
    int x = 0, y = 0;
    MsgTypes msgType;
    char *msg = new char[32];
    while(gameRunning)
    {
        if (isFirstPlayerTurn)
            msgType = play(p1_fd, P2Board, x, y);
        else
            msgType = play(p2_fd, P1Board, x, y);

        switch(msgType)
        {
            case HIT:
                break;

            case MISS:
                isFirstPlayerTurn = !isFirstPlayerTurn;
                break;

            case SINK:
                break;

            case WIN:
                gameRunning = false;
                break;

            default:
                break;
        }

        if (gameRunning)
        {
            sprintf(msg, "%d|%c%d||", msgType, (x + 65), y + 1);

            if (send(p1_fd, msg, sizeof(msg), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }
            if (send(p2_fd, msg, sizeof(msg), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }

            usleep(100);

            int sendTO = (isFirstPlayerTurn) ? p1_fd : p2_fd;
            if (send(sendTO, "1509||", sizeof("1509||"), 0) == -1){
                perror("Envoie du message : ");
                exit(EXIT_FAILURE);
            }
            cout << "Jeu en cours..." << endl;
        }
    }
    // endregion

    // region End Game
    
    cout<<entete<<"Game finished"<<endl;
    int winner = (isFirstPlayerTurn) ? p1_fd : p2_fd;
    int looser = (isFirstPlayerTurn) ? p2_fd : p1_fd;

    sprintf(msg, "%d|%c%d||", WIN, (x + 65), y + 1);
    if (send(winner, msg, sizeof(msg), 0) == -1) {
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }
    sprintf(msg, "%d|%c%d||", LOOSE, (x + 65), y + 1);
    if (send(looser, msg, sizeof(msg), 0) == -1) {
        perror("Envoie du message : ");
        exit(EXIT_FAILURE);
    }

    // endregion

    // region Desalloc
    
    cout<<entete<<"Stopping session"<<endl;
    for (int i = 0; i < boardSize; i++)
    {
        delete P1Board[i];
        delete P2Board[i];
    }
    delete[] P1Board;
    delete[] P2Board;

    // endregion

    return 0;
}
