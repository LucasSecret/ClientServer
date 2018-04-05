#ifndef BATTLESHIPS_MSGTYPES_H
#define BATTLESHIPS_MSGTYPES_H

enum MsgTypes
{
    LOGIN = 1001,
    LOGIN_SUCCESS = 1005,
    LOGIN_FAILED = 1006,
    SUBSCRIBE = 1010,
    SUBSCRIBE_SUCCESS = 1015,
    SUBSCRIBE_FAILED = 1016,

    ASK_INIT_GAME = 1050,

    INIT_GAME = 1100,
    INIT_SUCCESS = 1105,
    USER_NOT_FOUND = 1106,
    BOARD_TOO_SMALL = 1107,
    BOARD_TOO_BIG = 1108,

    JOIN_GAME = 1120,
    JOIN_SUCCESS = 1125,
    JOIN_FAILED = 1126,
    INIT_GRID = 1140,
    GRID_SUCCESS = 1145,
    ERR_BOAR_QTY = 1146,
    ERR_BOAT_SIZE = 1147,
    ERR_BOAT_OUTSIDE = 1148,

    START = 1500,

    FIRE = 1501,
    ERR_ALREADY_FIRED = 1502,
    HIT = 1503,
    MISS = 1504,
    SINK = 1505,
    ERR_COORDINATE = 1506,
    WIN = 1507,
    LOOSE = 1508,
    PLAY = 1509,

    AGAIN = 1701,
    LEAVE = 1705,
    ABORT = 1706,

    ERR_DISCONNECT = 9001,
    GAME_PAUSE = 9005,
    DISCONNECT = 9002,
    GAME_RUNNING = 9005,
    RESUME = 9006,
    RESUME_DATA = 9100,
    PLAYER_BACK = 9101
};

enum GameStatus
{
    WAITING_FOR_PLAYER,
    WAITING_FOR_GAME_INIT,
    WAITING_FOR_GRID_INIT,
    RUNNING
};

enum BoardElements
{
    EMPTY,
    BOAT,
    MISSED,
    HITTED,
    SINKED
};
#endif


//Notre enum:
#define CREATE_GAME 1
#define CONNECT 2
#define CREATE_BOAT 3
#define ATTACK 4
#define QUIT 5

#define GAME_OK 10
#define ID_TAKEN 11
#define SIZE_TOO_SMALL 12
#define UKNOWN_IP 13

#define CONNEXION_OK 20
#define CLIENT_JOINED 21
#define UKNOWN_GAME_ID 22

#define BOAT_OK 30
#define NEGATIVE_SIZE 30
#define BOAT_OUT_OF_BOUNDS 32
#define NON_STRAIGHT_BOAT 33
#define NOT_SPACE_ENOUGH 34

#define TOUCHED_BOAT 40
#define SINKED_BOAT 41
#define NOTHING_AFFECTED 42
#define GAME_WINNED 43
#define GAME_LOOSED 44
#define UKNOWN_TARGET 45
#define STRIKE_OUT_OF_BANDS 46

#define CLIENT_QUITED 50
