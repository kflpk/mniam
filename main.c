#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <netdb.h> 
#include <netinet/in.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


#include "amcom.h"
#include "amcom_packets.h"

typedef enum {
    FOOD_RIRST,
    FOOD_AND_BOTS_EQUAL
} GetMoveAngleMethod;

static const max_players = 8;
static const player_payload_size = 11;
static const food_state_size = sizeof(AMCOM_FoodState);
static const char* base_username = "rzujmi";
static char username[24] = "Rzujmi";
static uint8_t my_id = 0;

static AMCOM_PlayerState players[8];
static AMCOM_FoodState foods[128];

bool food_on_map(void) {
    for(size_t i = 0; i < sizeof(foods)/sizeof(AMCOM_FoodState); ++i)  {
        if(foods[i].state == 1) {
            return true;
        }
    }

    return false;
}

void parse_players(const AMCOM_Packet* packet) {
    if(packet == NULL)
        return;

    size_t size = packet->header.length;
    
    for(int i = 0; i < size / sizeof(AMCOM_PlayerState); i++) {
        AMCOM_PlayerState player = *((AMCOM_PlayerState*) &(packet->payload[i * player_payload_size]) );
        players[player.playerNo] = player;
        
    }
}

uint16_t get_player_hp(void);

void print_food_array(void) {
    for(size_t i = 0; i < sizeof(foods)/sizeof(AMCOM_FoodState); ++i) {
        AMCOM_FoodState food = foods[i];
        if(food.state != 0) {
            printf("foods[%d]\n\tfoodNo: %d\n\tstate: %d\n\tx: %f\n\ty: %f\n", i,(int)food.foodNo, (int)food.state, food.x, food.y);
        }
    }
}

size_t get_first_free_food_index(void) {
    for(size_t i = 0; i < sizeof(foods) / sizeof(AMCOM_FoodState); i++) {
        if(foods[i].state == 0) {
            return i;
        }
    }
    return -1;
}

void set_food_stat_to_eaten(uint16_t food_id) {
    for(size_t i = 0; i < sizeof(foods)/sizeof(AMCOM_FoodState); i++) {
        if(foods[i].foodNo == food_id) {
            // printf("Food with id: %d was eaten\n", i);
            fflush(stdout);
            foods[i].state = 0;
        }
    }
}

void parse_food(const AMCOM_Packet* packet) {
    
    size_t size = packet->header.length;
    size_t food_number = size / sizeof(AMCOM_FoodState);
    printf("food number: %d\n", food_number);
    fflush(stdout);

    for(size_t i = 0; i < food_number; i++) {
        AMCOM_FoodState food_state = *((AMCOM_FoodState*) &packet->payload[i * food_state_size]);

        printf("i = %d; foodNo: %d, state: %d\n", (int)i, (int)food_state.foodNo, (int)food_state.state);
        if(food_state.state != 0) {
            size_t first_free_food_index = get_first_free_food_index();
            printf("Fist food index: %d\n", first_free_food_index);

            if(first_free_food_index != -1) {
                foods[first_free_food_index] = food_state;
            }
        } else {
            set_food_stat_to_eaten(food_state.foodNo);
        }
    }
    fflush(stdout);
}


void initialize_food_array(void) {
    memset(foods, 0x00, sizeof(foods));
}

float get_nearest_smaller_enemy(float* enemyX, float* enemyY) {
    float nearest_yet_x = 1000*1.41;
    float nearest_yet_y = 1000*1.41;
    float smallestdistance = INFINITY;
    float px = 0, py = 0;

    for(size_t i = 0; i < sizeof(players)/sizeof(AMCOM_PlayerState); i++) {
        if(players[i].hp > 0 && players[i].playerNo != my_id && players[i].hp < get_player_hp()) {
            float ex = players[i].x;
            float ey = players[i].y;

            get_player_position(&px, &py);

            float vx = ex - px;
            float vy = ey - py;
            float distance = sqrt(vx*vx + vy*vy);
            if (distance < smallestdistance) {
                smallestdistance = distance;
                nearest_yet_x = ex;
                nearest_yet_y = ey;
                // fprintf(stdout, "x, y: %f, %f, distance: %f\n", fx, fy, distance);
            }
        }
    }
    
    *enemyX = nearest_yet_x;
    *enemyY = nearest_yet_y;
    return smallestdistance;
}

float get_nearest_food(float* foodX, float* foodY) {
    float nearest_yet_x = 1000*1.41;
    float nearest_yet_y = 1000*1.41;
    float smallestdistance = INFINITY;
    float px = 0, py = 0;

    for(size_t i = 0; i < sizeof(foods)/food_state_size; i++) {
        if(foods[i].state == 1) {
            float fx = foods[i].x;
            float fy = foods[i].y;

            get_player_position(&px, &py);

            float vx = fx - px;
            float vy = fy - py;
            float distance = sqrt(vx*vx + vy*vy);
            if (distance < smallestdistance) {
                smallestdistance = distance;
                nearest_yet_x = fx;
                nearest_yet_y = fy;
                // fprintf(stdout, "x, y: %f, %f, distance: %f\n", fx, fy, distance);
            }
        }
    }
    
    *foodX = nearest_yet_x;
    *foodY = nearest_yet_y;
    return smallestdistance;
}

void get_player_position(float* px, float* py) {
    for(uint8_t i = 0; i < sizeof(players); i++) {
        if(players[i].playerNo == my_id) {
            *px = players[i].x;
            *py = players[i].y;
            // fprintf(stdout, "px, py: %f, %f\n", *px, *py);
            return;
        }
    }
}

uint16_t get_player_hp(void) {
    for(uint8_t i = 0; i < sizeof(players); i++) {
        if(players[i].playerNo == my_id) {
            return players[i].hp;
        }
    }
}

float get_move_angle(GetMoveAngleMethod method) {
    float nearestFoodX = 0.0f;
    float nearestFoodY = 0.0f;
    float nearestEnemyX = 0.0f;
    float nearestEnemyY = 0.0f;
    float px, py; // player position
    float vx, vy; // vector pointing from player to food

    get_player_position(&px, &py);

    float food_distance = get_nearest_food(&nearestFoodX, &nearestFoodY);
    float enemy_distance = get_nearest_smaller_enemy(&nearestEnemyX, &nearestEnemyY);

    if(method == FOOD_RIRST) {
        get_nearest_food(&nearestFoodX, &nearestFoodY);
        vx = nearestFoodX - px;
        vy = nearestFoodY - py;
        if(!food_on_map()) {
            vx = nearestEnemyX - px;
            vy = nearestEnemyY - py;
        }
    } else if(method == FOOD_AND_BOTS_EQUAL) {
        if (enemy_distance < food_distance) {
            vx = nearestEnemyX - px;
            vy = nearestEnemyY - py;
        } else {
            vx = nearestFoodX - px;
            vy = nearestFoodY - py;
        }
    }


    return atan2(vy, vx);
}


void amPacketHandler(const AMCOM_Packet* packet, void* userContext) {

    uint8_t buf[AMCOM_MAX_PACKET_SIZE];
    int toSend = 0;
    static int first = 1;
    static bool players_set = false;


    // printf("Got an AMCOM packet with size: %d\n", packet->header.length);
    switch (packet->header.type) {
        case AMCOM_IDENTIFY_REQUEST: {
            printf("Got IDENTIFY.request. Responding with IDENTIFY.response\n");
            AMCOM_IdentifyResponsePayload identifyResponse;
            sprintf(identifyResponse.playerName, username);
            toSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), buf);
        } break;

        case AMCOM_NEW_GAME_REQUEST: 
            // reset internal game state
            printf("Got NEW_GAME.request.\n");
            AMCOM_NewGameRequestPayload newgameRequest;
            newgameRequest = *((AMCOM_NewGameRequestPayload*) (packet->payload));
            my_id = newgameRequest.playerNumber;
            initialize_food_array();

            AMCOM_NewGameResponsePayload newgameResponse;
            sprintf(newgameResponse.helloMessage, "Rzujmiiiii!!!");
            toSend = AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, &newgameResponse, sizeof(newgameResponse), buf);
        break;

        case AMCOM_PLAYER_UPDATE_REQUEST: 
            // printf("Got PLAYER_UPDATE.request\n");
            // print_food_array();
            memset(players, 0x00, sizeof(players));

            fflush(stdout);
            parse_players(packet);
        break;

        case AMCOM_FOOD_UPDATE_REQUEST: 
            printf("Got food request\n");
            parse_food(packet);

            fflush(stdout);
            // get_nearest_food(&nearestFoodX, &nearestFoodY);
        break;

        case AMCOM_MOVE_REQUEST: 
            // printf("Got MOVE.request\n");
            fflush(stdout);

            // print_food_array();
            AMCOM_MoveResponsePayload moveResponse;

            moveResponse.angle = get_move_angle(FOOD_AND_BOTS_EQUAL);

            toSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponse, sizeof(moveResponse), buf);
        break;

        case AMCOM_GAME_OVER_REQUEST: 
            // printf("Game over request\n");
            fflush(stdout);
            AMCOM_GameOverResponsePayload gameoverResponse;
            sprintf(gameoverResponse.endMessage, "smuteczek");
            toSend = AMCOM_Serialize(AMCOM_GAME_OVER_RESPONSE, &gameoverResponse, sizeof(gameoverResponse), buf);
        break;


        default: 
            // printf("Packet type: %d\n", packet->header.type);
        break;
    }

    int ConnectSocket = *((int*)userContext);
    int bytesSent = send(ConnectSocket, (const char*)buf, toSend, 0 );
    if (bytesSent == -1) {
        printf("Socket send failed with error\n");
        close(ConnectSocket);
        return;
    }
}


#define GAME_SERVER "127.0.0.1"
#define GAME_SERVER_PORT 2001

int main(int argc, char **argv) {
    printf("This is mniAM player. Let's eat some transistors! \n");

    struct sockaddr_in server;
    int ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectSocket <= -1){
        perror("Socket creation failed");
        return -1;
    }

    server.sin_addr.s_addr = inet_addr(GAME_SERVER);
    server.sin_family = AF_INET;
    server.sin_port = htons(GAME_SERVER_PORT);


    printf("Connecting to game server...\n");
    // Attempt to connect to an address until one succeeds
    if (connect(ConnectSocket, (struct sockaddr*)&server, sizeof(server)) != 0) {
        perror("Connection failed");
        return 1;
    }

    // Check if we connected to the game server
    if (ConnectSocket <= 0 ) {
        printf("Unable to connect to the game server!\n");
        return 1;
    }
    printf("Connected to game server\n");

    AMCOM_Receiver amReceiver;
    AMCOM_InitReceiver(&amReceiver, amPacketHandler, &ConnectSocket);

    // Receive until the peer closes the connection
    int iResult;
    char recvbuf[512];

    int recvbuflen = sizeof(recvbuf);
    do {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if ( iResult > 0 ) {
            AMCOM_Deserialize(&amReceiver, recvbuf, iResult);
        } else if ( iResult == 0 ) {
            printf("Connection closed\n");
        } else {
            printf("Recv failed with error: %d\n", iResult);
        }

    } while( iResult > 0 );

    // No longer need the socket
    close(ConnectSocket);

    return 0;
}
