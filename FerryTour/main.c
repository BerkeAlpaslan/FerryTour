#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define CAR 1
#define MINIBUS 2
#define TRUCK 3

#define VEHICLE_COUNT 30
#define FERRY_CAPACITY 20
#define SIDE_A 0
#define SIDE_B 1

typedef struct {
    int id;
    int type;
    int load;
    int side;
    int targetSide;
    int returned;
} Vehicle;

pthread_mutex_t toll_mutex[4];
pthread_mutex_t square_mutex[2];
pthread_mutex_t ferry_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ferry_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t ferry_empty = PTHREAD_COND_INITIALIZER;

Vehicle* ferry[10];
int ferry_load = 0;
int ferryVehicleCount = 0;
int ferrySide = SIDE_A;

Vehicle* square[2][VEHICLE_COUNT];
int squareCount[2] = {0,0};

Vehicle vehicles[VEHICLE_COUNT];
pthread_t vehicleThreads[VEHICLE_COUNT];

int finished = 0;

void init_vehicles() {
    int i, j = 0;

    for (i = 0; i < 12; i++) {
        vehicles[j++] = (Vehicle) {j, CAR, 1, rand() % 2, -1, 0};
    }

    for (i = 0; i < 10; i++) {
        vehicles[j++] = (Vehicle) {j, MINIBUS, 2, rand() % 2, -1, 0};
    }

    for (i = 0; i < 8; i++) {
        vehicles[j++] = (Vehicle) {j, TRUCK, 3, rand() % 2, -1, 0};
    }
}

void wait_random() {
    usleep((rand() % 500 + 200) * 1000);
}

void pass_toll(Vehicle* vehicle) {
    int tollIndex = rand() % 2;
    pthread_mutex_lock(&toll_mutex[vehicle->side * 2 + tollIndex]);
    printf("The vehicle #%d passed from toll. (Type of Vehicle: %s, Side: %c)", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", vehicle->side == 0 ? 'A' : 'B');
    wait_random();
    pthread_mutex_unlock(&toll_mutex[vehicle->side * 2 + tollIndex]);
}
void wait_square(Vehicle* vehicle) {
    pthread_mutex_lock(&square_mutex[vehicle->side]);
    square[vehicle->side][squareCount[vehicle->side]++] = vehicle;
    pthread_mutex_unlock(&square_mutex[vehicle->side]);
}

void board_ferry(Vehicle* vehicle) {
    pass_toll(vehicle);
    wait_square(vehicle);

    while (1) {
        pthread_mutex_lock(&ferry_mutex);

        if (vehicle->side == ferrySide && ferry_load + vehicle->load <= FERRY_CAPACITY) {
            // Get on the ferry
            ferry[ferryVehicleCount++] = vehicle;
            ferry_load += vehicle->load;

            // Exit from the square to ferry
            pthread_mutex_lock(&square_mutex[vehicle->side]);
            for (int i = 0; i < squareCount[vehicle->side]; i++) {
                if (square[vehicle->side][i] == vehicle) {
                    for (int j = i; j < squareCount[vehicle->side] - 1; j++) {
                        square[vehicle->side][j] = square[vehicle->side][j + 1];
                    }
                    squareCount[vehicle->side]--;
                    break;
                }
            }
            pthread_mutex_unlock(&square_mutex[vehicle->side]);

            printf("The vehicle #%d got on the ferry. (Type of Vehicle: %s, Side: %c)", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", vehicle->side == 0 ? 'A' : 'B');

            if (ferry_load >= FERRY_CAPACITY) {
                pthread_cond_signal(&ferry_full);
            }

            pthread_mutex_unlock(&ferry_mutex);
            break;
        }

        pthread_mutex_unlock(&ferry_mutex);
        usleep(100000);
    }
}

void* vehicle_fn(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;

    // Departure
    pass_toll(vehicle);
    wait_square(vehicle);

    while (1) {
        pthread_mutex_lock(&ferry_mutex);

        if (vehicle->side == ferrySide && ferry_load + vehicle->load <= FERRY_CAPACITY) {
            // Get on the ferry
            ferry[ferryVehicleCount++] = vehicle;
            ferry_load += vehicle->load;

            // Exit from the square to ferry
            pthread_mutex_lock(&square_mutex[vehicle->side]);
            for (int i = 0; i < squareCount[vehicle->side]; i++) {
                if (square[vehicle->side][i] == vehicle) {
                    for (int j = i; j < squareCount[vehicle->side] - 1; j++) {
                        square[vehicle->side][j] = square[vehicle->side][j + 1];
                    }
                    squareCount[vehicle->side]--;
                    break;
                }
            }
            pthread_mutex_unlock(&square_mutex[vehicle->side]);

            printf("The vehicle #%d got on the ferry. (Type of Vehicle: %s, Side: %c)", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", vehicle->side == 0 ? 'A' : 'B');

            if (ferry_load >= FERRY_CAPACITY) {
                pthread_cond_signal(&ferry_full);
            }

            pthread_mutex_unlock(&ferry_mutex);
            break;
        }

        pthread_mutex_unlock(&ferry_mutex);
        usleep(100000);
    }

    // Get off the ferry
    while (1) {
        pthread_mutex_lock(&ferry_mutex);
        if (vehicle->side != ferrySide) {
            printf("The vehicle #%d got off from ferry. (Type of Vehicle: %s, Side: %c)", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", ferrySide == 0 ? 'A' : 'B');
            vehicle->side = ferrySide;
            pthread_mutex_unlock(&ferry_mutex);
            break;
        }
        pthread_mutex_unlock(&ferry_mutex);
        usleep(100000);
    }

    // Return
    wait_random();
    if (!vehicle-> returned) {
        vehicle-> returned = 1;
        vehicle->targetSide = vehicle->side == SIDE_A ? SIDE_B : SIDE_A;
        pass_toll(vehicle);
        wait_square(vehicle);

        while (1) {
            pthread_mutex_lock(&ferry_mutex);
            if (vehicle->side == ferrySide && ferry_load + vehicle->load <= FERRY_CAPACITY) {
                ferry[ferryVehicleCount++] = vehicle;
                ferry_load += vehicle->load;

                pthread_mutex_lock(&square_mutex[vehicle->side]);
                for (int i = 0; i < squareCount[vehicle->side]; i++) {
                    if (square[vehicle->side][i] == vehicle) {
                        for (int j = i; j < squareCount[vehicle->side] - 1; j++) {
                            square[vehicle->side][j] = square[vehicle->side][j + 1];
                        }
                        squareCount[vehicle->side]--;
                        break;
                    }
                }
                pthread_mutex_unlock(&square_mutex[vehicle->side]);

                printf("The vehicle #%d got on the ferry for return. (Type of vehicle: %s, Side: %c)\n", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", vehicle->side == 0 ? 'A' : 'B');

                if (ferry_load >= FERRY_CAPACITY) {
                    pthread_cond_signal(&ferry_full);
                }

                pthread_mutex_unlock(&ferry_mutex);
                break;
            }

            pthread_mutex_unlock(&ferry_mutex);
            usleep(100000);
        }

        while (1) {
            pthread_mutex_lock(&ferry_mutex);
            if (vehicle->side != ferrySide) {
                printf("The vehicle #%d got off from ferry on the way back. (Type of Vehicle: %s, Side: %c)", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", ferrySide == 0 ? 'A' : 'B');
                vehicle->side = ferrySide;
                pthread_mutex_unlock(&ferry_mutex);
                break;
            }
            pthread_mutex_unlock(&ferry_mutex);
            usleep(100000);
        }
    }

    return NULL;
}

int main(void) {
    printf("Hello, World!\n");
    return 0;
}