#include "ferry.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t toll_mutex[4];
pthread_mutex_t square_mutex[2];
pthread_mutex_t ferry_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ferry_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t ferry_empty = PTHREAD_COND_INITIALIZER;

Vehicle* ferry[10];
int ferryLoad = 0;
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
        vehicles[j++] = (Vehicle) {j, CAR, 1, SIDE_A, SIDE_B, 0};
    }

    for (i = 0; i < 10; i++) {
        vehicles[j++] = (Vehicle) {j, MINIBUS, 2, SIDE_A, SIDE_B, 0};
    }

    for (i = 0; i < 8; i++) {
        vehicles[j++] = (Vehicle) {j, TRUCK, 3, SIDE_A, SIDE_B, 0};
    }
}

void wait_random() {
    usleep((rand() % 500 + 200) * 1000);
}

int existsSuitableVehicle() {
    for (int i = 0; i < squareCount[ferrySide]; i++) {
        if (square[ferrySide][i]->load + ferryLoad <= FERRY_CAPACITY) {
            return 1;
        }
    }
    return 0;
}

void pass_toll(Vehicle* vehicle) {
    int toll_index = rand() % 2;
    pthread_mutex_lock(&toll_mutex[vehicle->side * 2 + toll_index]);
    printf("The vehicle #%d passed from toll. (Type of Vehicle: %s, Side: %c)",
        vehicle->id, vehicle->type == CAR ? "CAR" : vehicle->type == MINIBUS ? "MINIBUS" : "TRUCK",
        vehicle->side);
    pthread_mutex_unlock(&toll_mutex[vehicle->side * 2 + toll_index]);
}

void wait_square(Vehicle* vehicle) {
    pthread_mutex_lock(&square_mutex[vehicle->side]);
    square[vehicle->side][squareCount[vehicle->side]++] = vehicle;
    pthread_mutex_unlock(&square_mutex[vehicle->side]);
}

void* boarding_ferry(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;

    while (1) {
        pthread_mutex_lock(&ferry_mutex);
        if (!vehicle->returned && vehicle->side == ferrySide && vehicle->targetSide != ferrySide && ferryLoad + vehicle->load <= FERRY_CAPACITY) {
            pthread_mutex_lock(&square_mutex[vehicle->side]);
            for (int i = 0; i < squareCount[vehicle->side]; i++) {
                if (square[vehicle->side][i] == vehicle) {
                    for (int j = i; j < squareCount[vehicle->side] - 1; j++) {
                        square[vehicle->side][j] = square[vehicle->side][j + 1];
                    }
                    if (vehicle->targetSide == SIDE_A) {
                        vehicle->returned = 1;
                    }
                    squareCount[vehicle->side]--;
                    break;
                }
            }
            pthread_mutex_unlock(&square_mutex[vehicle->side]);

            ferry[ferryVehicleCount++] = vehicle;
            ferryLoad += vehicle->load;

            printf("The vehicle #%d got on the ferry. (Type of Vehicle: %s, Side: %c)", vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", vehicle->side == 0 ? 'A' : 'B');

            if (ferryLoad >= FERRY_CAPACITY) {
                pthread_cond_signal(&ferry_full);
            }

            pthread_mutex_unlock(&ferry_mutex);
            break;
        }
    }

    pthread_mutex_unlock(&ferry_mutex);
    usleep(100000);

    return NULL;
}

void* ferry_departure() {
    pthread_mutex_lock(&ferry_mutex);
    while (ferryLoad < FERRY_CAPACITY && squareCount[ferrySide] && existsSuitableVehicle()) {
        pthread_cond_wait(&ferry_full, &ferry_mutex);
    }
    printf("Ferry is departing (%c -> %c)",
           ferrySide == 0 ? 'A' : 'B',
           ferrySide == 0 ? 'B' : 'A');
    pthread_mutex_unlock(&ferry_mutex);

    sleep(5);

    pthread_mutex_lock(&ferry_mutex);
    ferrySide = !ferrySide;

    ferryLoad = 0;
    ferryVehicleCount = 0;
    pthread_cond_broadcast(&ferry_empty);
    pthread_mutex_unlock(&ferry_mutex);

    return NULL;
}

void* leaving_ferry(Vehicle* vehicle) {
    while (1) {
        pthread_mutex_lock(&ferry_mutex);
        if (vehicle->side != ferrySide && vehicle->targetSide == ferrySide) {
            printf("The vehicle #%d got off from ferry. (Type of Vehicle: %s, Side: %c)",
                vehicle->id,
                vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK", ferrySide == 0 ? 'A' : 'B');
            vehicle->targetSide = vehicle->side;
            vehicle->side = ferrySide;
            squareCount[vehicle->side]++;
            pthread_mutex_unlock(&ferry_mutex);
            break;
        }
        pthread_mutex_unlock(&ferry_mutex);
        usleep(100000);
    }

    return NULL;
}