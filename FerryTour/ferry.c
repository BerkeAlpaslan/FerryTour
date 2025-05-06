#include "ferry.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t toll_mutex[4];
pthread_mutex_t square_mutex[2];
pthread_mutex_t ferry_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ferry_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t ferry_empty = PTHREAD_COND_INITIALIZER;

Vehicle* ferry[FERRY_CAPACITY];
int ferryLoad = 0;
int ferryVehicleCount = 0;
int ferrySide = SIDE_A;

Vehicle* square[2][VEHICLE_COUNT];
int squareCount[2] = {0,0};

Vehicle vehicles[VEHICLE_COUNT];
pthread_t vehicleThreads[VEHICLE_COUNT];

int finished = 0;

void init_vehicles() {
    int i;
    int j = 0;
    int side;

    for (i = 0; i < 12; i++) {
        side = rand() % 2;
        vehicles[j++] = (Vehicle) {j, CAR, 1, side, side, side == 0 ? SIDE_B : SIDE_A, 0};
    }

    for (i = 0; i < 10; i++) {
        side = rand() % 2;
        vehicles[j++] = (Vehicle) {j, MINIBUS, 2, side, side, side == 0 ? SIDE_B : SIDE_A, 0};
    }

    for (i = 0; i < 8; i++) {
        side = rand() % 2;
        vehicles[j++] = (Vehicle) {j, TRUCK, 3, side, side, side == 0 ? SIDE_B : SIDE_A, 0};
    }
}

void wait_random() {
    usleep((rand() % 500 + 200) * 1000);
}

int existsSuitableVehicleOnSide() {
    for (int i = 0; i < squareCount[ferrySide]; i++) {
        if (square[ferrySide][i]->load + ferryLoad <= FERRY_CAPACITY && square[ferrySide][i]->returned == 0) {
            return 1;
        }
    }
    return 0;
}

int existsSuitableVehicleOtherSide() {
    int otherSide = !ferrySide;

    if (ferryLoad == 0 && ferryVehicleCount == 0 && squareCount[otherSide] > 0) {
        for (int i = 0; i < squareCount[otherSide]; i++) {
            if (square[otherSide][i]->returned == 0) {
                return 1;
            }
        }
    }

    return 0;
}

int all_returned() {
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        if (vehicles[i].returned == 0) {
            return 0;
        }
    }
    return 1;
}

void pass_toll(Vehicle* vehicle) {
    int toll_index = rand() % 2;
    pthread_mutex_lock(&toll_mutex[vehicle->current_side * 2 + toll_index]);
    printf("The vehicle #%d passed from toll. (Type of Vehicle: %s, Side: %c)",
        vehicle->id, vehicle->type == CAR ? "CAR" : vehicle->type == MINIBUS ? "MINIBUS" : "TRUCK",
        vehicle->current_side);
    wait_random();
    pthread_mutex_unlock(&toll_mutex[vehicle->current_side * 2 + toll_index]);
}

void wait_square(Vehicle* vehicle) {
    pthread_mutex_lock(&square_mutex[vehicle->current_side]);
    square[vehicle->current_side][squareCount[vehicle->current_side]++] = vehicle;
    printf("Vehicle #%d waiting in square. (Type: %s, Side: %c)\n",
        vehicle->id,
        vehicle->type == CAR ? "CAR" : vehicle->type == MINIBUS ? "MINIBUS" : "TRUCK",
        vehicle->current_side == SIDE_A ? 'A' : 'B');
    pthread_mutex_unlock(&square_mutex[vehicle->current_side]);
}

void* boarding_ferry(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;
    int boarded = 0;

    while (!boarded) {
        pthread_mutex_lock(&ferry_mutex);
        if (!vehicle->returned && vehicle->current_side == ferrySide && vehicle->targetSide != ferrySide && ferryLoad + vehicle->load <= FERRY_CAPACITY) {
            pthread_mutex_lock(&square_mutex[vehicle->current_side]);
            for (int i = 0; i < squareCount[vehicle->current_side]; i++) {
                if (square[vehicle->current_side][i] == vehicle) {
                    for (int j = i; j < squareCount[vehicle->current_side] - 1; j++) {
                        square[vehicle->current_side][j] = square[vehicle->current_side][j + 1];
                    }
                    if (vehicle->targetSide == SIDE_A) {
                        vehicle->returned = 1;
                    }
                    squareCount[vehicle->current_side]--;
                    break;
                }
            }
            pthread_mutex_unlock(&square_mutex[vehicle->current_side]);

            ferry[ferryVehicleCount++] = vehicle;
            ferryLoad += vehicle->load;

            printf("The vehicle #%d got on the ferry. (Type of Vehicle: %s, Side: %c)",
                vehicle->id, vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK",
                vehicle->current_side == 0 ? 'A' : 'B');

            if (ferryLoad == FERRY_CAPACITY || (ferryLoad >= 0 && !existsSuitableVehicleOnSide())) {
                pthread_cond_signal(&ferry_full);
            }

            boarded = 1;

            pthread_mutex_unlock(&ferry_mutex);
            break;
        }

        pthread_mutex_unlock(&ferry_mutex);

        if (!boarded) {
            usleep(100000);
        }
    }

    return NULL;
}

void* ferry_departure() {
    pthread_mutex_lock(&ferry_mutex);

    if (ferryLoad == 0) {
        if (squareCount[ferrySide] == 0 && !existsSuitableVehicleOnSide() && existsSuitableVehicleOtherSide()) {
            printf("Ferry is departing EMPTY from Side %c to Side %c\n",
                ferrySide == SIDE_A ? 'A' : 'B',
                ferrySide == SIDE_A ? 'B' : 'A');

            pthread_mutex_unlock(&ferry_mutex);
            sleep(5);
            pthread_mutex_lock(&ferry_mutex);
            ferrySide = !ferrySide;
            pthread_mutex_unlock(&ferry_mutex);

            return NULL;
        } else if (squareCount[ferrySide] > 0 && existsSuitableVehicleOnSide()) {
            while (ferryVehicleCount == 0 && squareCount[ferrySide] > 0) {
                pthread_mutex_unlock(&ferry_mutex);
                usleep(500000);
                pthread_mutex_lock(&ferry_mutex);
            }
        } else {
            pthread_mutex_unlock(&ferry_mutex);
            return NULL;
        }
    }

    if (ferryVehicleCount > 0 && (ferryLoad == FERRY_CAPACITY || !existsSuitableVehicleOnSide())) {
        printf("Ferry is departing from Side %c to Side %c with %d vehicles (Load: %d/%d)\n",
            ferrySide == SIDE_A ? 'A' : 'B',
            ferrySide == SIDE_A ? 'B' : 'A',
            ferryVehicleCount, ferryLoad, FERRY_CAPACITY);

        pthread_mutex_unlock(&ferry_mutex);
        sleep(5);
        pthread_mutex_lock(&ferry_mutex);
        ferrySide = !ferrySide;

        for (int i = 0; i < ferryVehicleCount; i++) {
            Vehicle* v = ferry[i];
            printf("Vehicle #%d got off from ferry. (Type: %s, Side: %c)\n",
                v->id,
                v->type == CAR ? "CAR" : v->type == MINIBUS ? "MINIBUS" : "TRUCK",
                ferrySide == SIDE_A ? 'A' : 'B');

            v->current_side = ferrySide;

            if (v->current_side == v->initial_side) {
                v->returned = 1;
                printf("Vehicle #%d has returned to its initial side. (Side: %c)\n",
                    v->id, ferrySide == SIDE_A ? 'A' : 'B');
            }

            v->targetSide = v->current_side == SIDE_A ? SIDE_B : SIDE_A;
        }

        ferryLoad = 0;
        ferryVehicleCount = 0;

        pthread_cond_broadcast(&ferry_empty);
        pthread_mutex_unlock(&ferry_mutex);
    } else {
        pthread_mutex_unlock(&ferry_mutex);
        usleep(500000);
    }

    return NULL;
}

void* leaving_ferry(Vehicle* vehicle) {
    while (1) {
        pthread_mutex_lock(&ferry_mutex);
        if (ferrySide == vehicle->targetSide) {  // Araç hedef yanına ulaştı
            printf("The vehicle #%d got off from ferry. (Type of Vehicle: %s, Side: %c)\n",
                vehicle->id,
                vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK",
                ferrySide == 0 ? 'A' : 'B');

            vehicle->current_side = ferrySide;
            if (vehicle->current_side == vehicle->initial_side) {
                vehicle->returned = 1;
                printf("Vehicle #%d has returned to its initial side. (Side: %c)\n",
                    vehicle->id, ferrySide == 0 ? 'A' : 'B');
            }
            vehicle->targetSide = vehicle->current_side == SIDE_A ? SIDE_B : SIDE_A;

            pthread_mutex_unlock(&ferry_mutex);

            pthread_mutex_lock(&square_mutex[vehicle->current_side]);
            square[vehicle->current_side][squareCount[vehicle->current_side]] = vehicle;
            squareCount[vehicle->current_side]++;
            pthread_mutex_unlock(&square_mutex[vehicle->current_side]);

            break;
        }
        pthread_mutex_unlock(&ferry_mutex);
        usleep(100000);
    }

    return NULL;
}