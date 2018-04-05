#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>

#include "common/enums.h"

#define MYPORT 44578
#define BACKLOG 10
#define MAXDATASIZE 200

int boardSize;

void initGame(int socket);
void initBoard(int socket, BoardElements ** board);

bool fillBoard(BoardElements ** board, const std::string &places);
bool getCoordinate(const std::string &places, unsigned long & lastIndex, int & x, int & y);
bool checkBoardValidity(int socket, BoardElements ** board);

MsgTypes play(int socket, BoardElements ** adversaryBoard, int &x, int &y);
bool getFireCoordinate(const std::string &message, unsigned long nextDelimiter, int &x, int &y);
bool boatSinked(int x, int y, BoardElements ** adversaryBoard);
bool boardContainsBoat(BoardElements ** adversaryBoard);

int getMessageID(const std::string &message, unsigned long &nextParameterPos);

int main()
{
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
    //endregion

    //region First User Connection
    int player1_fd{};
    struct sockaddr_in addr{};
    socklen_t sin_size = sizeof(struct sockaddr);

    if(listen(sockfd, BACKLOG) == -1)
    {
        perror("Listen socket : ");
        exit(EXIT_FAILURE);
    }

    if((player1_fd = accept(sockfd, (struct sockaddr *)&addr, &sin_size)) == -1)
    {
        perror("Accept premier client : ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (send(player1_fd, "1050||", sizeof("1050||"), 0) == -1){
        perror("send message : ");
        exit(EXIT_FAILURE);
    }
    //endregion

    std::thread init_game(initGame, player1_fd);

    //region Second User Connection
    int player2_fd{};
    if((player2_fd = accept(sockfd, (struct sockaddr *)&addr, &sin_size)) == -1)
    {
        perror("Accept second client : ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    //endregion

    //region Init Game & Board
    init_game.join();

    char buf[20];
    sprintf(buf, "%d|%d||", JOIN_SUCCESS, boardSize);

    if (send(player1_fd, buf, sizeof(buf), 0) == -1){
        perror("send message : ");
        exit(EXIT_FAILURE);
    }
    if (send(player2_fd, buf, sizeof(buf), 0) == -1){
        perror("send message : ");
        exit(EXIT_FAILURE);
    }

    auto ** firstPlayerBoard = new BoardElements*[boardSize];
    auto ** secondPlayerBoard = new BoardElements*[boardSize];

    for (int i = 0; i < boardSize; i++)
    {
        firstPlayerBoard[i] = new BoardElements[boardSize];
        secondPlayerBoard[i] = new BoardElements[boardSize];
    }

    std::thread init_player1(initBoard, player1_fd, firstPlayerBoard);
    std::thread init_player2(initBoard, player2_fd, secondPlayerBoard);

    init_player1.join();
    init_player2.join();

    // endregion

    // region Launch Game

    char startMessagePlayer1[11];
    char startMessagePlayer2[11];
    bool isFirstPlayerTurn = (random() % 2 == 0);

    if (isFirstPlayerTurn)
    {
        strcpy(startMessagePlayer1, "1500|true||");
        strcpy(startMessagePlayer2, "1500|false||");
    }
    else
    {
        strcpy(startMessagePlayer2, "1500|true||");
        strcpy(startMessagePlayer1, "1500|false||");
    }

    if (send(player1_fd, startMessagePlayer1, sizeof(startMessagePlayer1), 0) == -1){
        perror("send message : ");
        exit(EXIT_FAILURE);
    }
    if (send(player2_fd, startMessagePlayer2, sizeof(startMessagePlayer2), 0) == -1){
        perror("send message : ");
        exit(EXIT_FAILURE);
    }

    // endregion

    // region Game Running
    bool gameRunning = true;
    int x = 0, y = 0;
    MsgTypes msgType;
    auto * message = new char[32];
    while(gameRunning)
    {
        if (isFirstPlayerTurn)
            msgType = play(player1_fd, secondPlayerBoard, x, y);
        else
            msgType = play(player2_fd, firstPlayerBoard, x, y);

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
            sprintf(message, "%d|%c%d||", msgType, (x + 65), y + 1);

            if (send(player1_fd, message, sizeof(message), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            if (send(player2_fd, message, sizeof(message), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }

            usleep(100);

            int sendTO = (isFirstPlayerTurn) ? player1_fd : player2_fd;
            if (send(sendTO, "1509||", sizeof("1509||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            std::cout << "PLAY" << std::endl;
        }
    }
    // endregion

    // region End Game

    int winner = (isFirstPlayerTurn) ? player1_fd : player2_fd;
    int looser = (isFirstPlayerTurn) ? player2_fd : player1_fd;

    sprintf(message, "%d|%c%d||", WIN, (x + 65), y + 1);
    if (send(winner, message, sizeof(message), 0) == -1) {
        perror("send message : ");
        exit(EXIT_FAILURE);
    }
    sprintf(message, "%d|%c%d||", LOOSE, (x + 65), y + 1);
    if (send(looser, message, sizeof(message), 0) == -1) {
        perror("send message : ");
        exit(EXIT_FAILURE);
    }

    // endregion

    // region Desalloc

    for (int i = 0; i < boardSize; i++)
    {
        delete firstPlayerBoard[i];
        delete secondPlayerBoard[i];
    }
    delete[] firstPlayerBoard;
    delete[] secondPlayerBoard;

    // endregion

    return 0;
}

void initGame(int socket)
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
            case INIT_GAME:
                temp = string.substr(nextDelimiter, string.find('|', nextDelimiter) - nextDelimiter);
                break;

            default:
                std::cout << "Invalid Message " << message << std::endl;
                std::cout.flush();
                continue;
        }

        try {
            boardSize = stoi(temp);
        }
        catch (std::exception &e) {
            if (send(socket, "1107||", sizeof("1107||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (boardSize < 3)
        {
            if (send(socket, "1107||", sizeof("1107||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }
        else if (boardSize > 20)
        {
            if (send(socket, "1108||", sizeof("1108||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        sprintf(message, "%d||", INIT_SUCCESS);
        if (send(socket, message, sizeof(message), 0) == -1){
            perror("send message : ");
            exit(EXIT_FAILURE);
        }

        return;
    }
}

void initBoard(int socket, BoardElements ** board)
{
    char message[MAXDATASIZE];

    while (true) {
        bzero(message, MAXDATASIZE);
        if (recv(socket, &message, MAXDATASIZE, MSG_WAITALL) == -1) {
            perror("Recv Error");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < boardSize; i++)
            for (int j = 0; j < boardSize; j++)
                board[i][j] = EMPTY;

        std::string string(message);
        unsigned long nextDelimiter;
        int id;
        std::string temp;

        if ((id = getMessageID(string, nextDelimiter)) == -1)
            continue;

        switch (id) {
            case INIT_GRID:
                temp = string.substr(nextDelimiter, string.find('|', nextDelimiter) - nextDelimiter);
                break;

            default:
                std::cout << "Invalid Message " << message << std::endl;
                std::cout.flush();
                continue;
        }

        if (!fillBoard(board, temp))
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
            perror("send message : ");
            exit(EXIT_FAILURE);
        }
        return false;
    }

    return true;
}

MsgTypes play(int socket, BoardElements ** adversaryBoard, int &x, int &y)
{
    char message[MAXDATASIZE];

    while (true)
    {
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
            case FIRE:
                if (!getFireCoordinate(message, nextDelimiter, x, y))
                {
                    if (send(socket, "1506||", sizeof("1506||"), 0) == -1){
                        perror("send message : ");
                        exit(EXIT_FAILURE);
                    }
                    continue;
                }
                break;

            default:
                std::cout << "Invalid Message " << message << std::endl;
                std::cout.flush();
                continue;
        }

        if (adversaryBoard[x][y] == MISSED || adversaryBoard[x][y] == HITTED || adversaryBoard[x][y] == SINKED)
        {
            if (send(socket, "1502||", sizeof("1502||"), 0) == -1){
                perror("send message : ");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        std::string result;

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
bool boatSinked(int x, int y, int addX, int addY, BoardElements ** adversaryBoard)
{
    if (x + addX < 0 || x + addX >= boardSize || y + addY < 0 && y + addY >= boardSize)
        return true;

    BoardElements element = adversaryBoard[x + addX][y + addY];

    if (element == BOAT)
        return false;
    if (element == HITTED)
        return boatSinked(x + addX, y + addY, addX, addY, adversaryBoard);

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
        return boatSinked(x, y, 1, 0, adversaryBoard) && boatSinked(x, y, -1, 0, adversaryBoard);
    }
    else if ((y + 1 < boardSize && adversaryBoard[x][y + 1] == HITTED) ||
             (y - 1 >= 0        && adversaryBoard[x][y - 1] == HITTED))
    {
        return boatSinked(x, y, 0, 1, adversaryBoard) && boatSinked(x, y, 0, -1, adversaryBoard);
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
