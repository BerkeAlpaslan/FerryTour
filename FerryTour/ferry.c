#include "ferry.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t toll_mutex[4];
pthread_mutex_t square_mutex[2];
pthread_mutex_t ferry_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t ferry_available = PTHREAD_COND_INITIALIZER;
pthread_cond_t departure_available = PTHREAD_COND_INITIALIZER;

pthread_t ferry_departure_thread;
pthread_t vehicleThreads[VEHICLE_COUNT];
Vehicle vehicles[VEHICLE_COUNT];

Ferry ferry;

Vehicle* ferry_vehicles[FERRY_CAPACITY];
Vehicle* square[2][VEHICLE_COUNT];

int square_count[2] = {0,0};

int ferry_available_status = 1;
int departure_available_status = 0;

void init_variables() {
    int i;
    int j = 0;
    int side = rand() % 2;

    // Init ferry
    ferry = (Ferry) {0, 0, side, !side};

    // Init 12 cars
    for (i = 0; i < 12; i++) {
        side = rand() % 2;
        vehicles[j++] = (Vehicle) {j, CAR, 1, side, side, side == 0 ? SIDE_B : SIDE_A, 0, 0};
    }

    // Init 10 minibuses
    for (i = 0; i < 10; i++) {
        side = rand() % 2;
        vehicles[j++] = (Vehicle) {j, MINIBUS, 2, side, side, side == 0 ? SIDE_B : SIDE_A, 0, 0};
    }

    // Init 8 trucks
    for (i = 0; i < 8; i++) {
        side = rand() % 2;
        vehicles[j++] = (Vehicle) {j, TRUCK, 3, side, side, side == 0 ? SIDE_B : SIDE_A, 0, 0};
    }
}

int existsSuitableVehicleOnSide() {
    int result = 0;

    pthread_mutex_lock(&square_mutex[ferry.current_side]);

    for (int i = 0; i < square_count[ferry.current_side]; i++) {
        if (square[ferry.current_side][i] != NULL &&
            square[ferry.current_side][i]->target_side == ferry.target_side &&
            square[ferry.current_side][i]->load + ferry.load <= FERRY_CAPACITY &&
            !square[ferry.current_side][i]->returned &&
            !square[ferry.current_side][i]->on_ferry) {
            result = 1;
            break;
            }
    }

    pthread_mutex_unlock(&square_mutex[ferry.current_side]);
    return result;
}

int existsSuitableVehicleOtherSide() {
    int result = 0;

    pthread_mutex_lock(&square_mutex[ferry.target_side]);

    if (ferry.load == 0 && ferry.vehicle_count == 0 && square_count[ferry.target_side] > 0) {
        for (int i = 0; i < square_count[ferry.target_side]; i++) {
            if (square[ferry.target_side][i] != NULL &&
                !square[ferry.target_side][i]->returned &&
                !square[ferry.target_side][i]->on_ferry) {
                result = 1;
                break;
                }
        }
    }

    pthread_mutex_unlock(&square_mutex[ferry.target_side]);
    return result;
}

int all_returned() {
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        if (!vehicles[i].returned) {
            return 0;
        }
    }
    return 1;
}

void swap_vehicle_side(Vehicle* vehicle) {
    if (vehicle != NULL) {
        int temp;
        temp = vehicle->current_side;
        vehicle->current_side = vehicle->target_side;
        vehicle->target_side = temp;

        if (vehicle->current_side == vehicle->initial_side) {
            vehicle->returned = 1;
            printf("Vehicle #%d has returned to its initial side. (Side: %c)\n",
                vehicle->id, vehicle->initial_side == SIDE_A ? 'A' : 'B');
        }
    }
}

void swap_ferry_side(Ferry* ferry) {
    if (ferry != NULL) {
        int temp;
        temp = ferry->current_side;
        ferry->current_side = ferry->target_side;
        ferry->target_side = temp;
    }
}

void pass_toll(Vehicle* vehicle) {
    int toll_index = rand() % 2;

    pthread_mutex_lock(&toll_mutex[vehicle->current_side * 2 + toll_index]);

    printf("The vehicle #%d passed from toll. (Type of Vehicle: %s, Side: %c)\n",
        vehicle->id, vehicle->type == CAR ? "CAR" : vehicle->type == MINIBUS ? "MINIBUS" : "TRUCK",
        vehicle->current_side == 0 ? 'A' : 'B');

    usleep(10000);

    pthread_mutex_unlock(&toll_mutex[vehicle->current_side * 2 + toll_index]);
}

void wait_square(Vehicle* vehicle) {
    pthread_mutex_lock(&square_mutex[vehicle->current_side]);

    square[vehicle->current_side][square_count[vehicle->current_side]++] = vehicle;
    printf("Vehicle #%d waiting in square. (Type: %s, Side: %c)\n",
        vehicle->id,
        vehicle->type == CAR ? "CAR" : vehicle->type == MINIBUS ? "MINIBUS" : "TRUCK",
        vehicle->current_side == SIDE_A ? 'A' : 'B');
    usleep(10000);

    pthread_mutex_unlock(&square_mutex[vehicle->current_side]);
}

void leaving_ferry() {
    for (int i = 0; i < ferry.vehicle_count; i++) {
        Vehicle* vehicle = ferry_vehicles[i];
        if (vehicle != NULL) {
            printf("Vehicle #%d got off from ferry. (Type: %s, Side: %c)\n",
                vehicle->id,
                vehicle->type == CAR ? "CAR" : vehicle->type == MINIBUS ? "MINIBUS" : "TRUCK",
                ferry.current_side == SIDE_A ? 'A' : 'B');

            swap_vehicle_side(vehicle);
            vehicle->on_ferry = 0;
            ferry_vehicles[i] = NULL;
        }
    }

    ferry.load = 0;
    ferry.vehicle_count = 0;
}

void boarding_ferry(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;

    pthread_mutex_lock(&ferry_mutex);

    // Wait for ferry to be available
    while (!ferry_available_status || vehicle->current_side != ferry.current_side) {
        pthread_cond_wait(&ferry_available, &ferry_mutex);
    }

    // Set statuses to prevent other vehicles from boarding simultaneously
    ferry_available_status = 0;

    // Check if vehicle can board
    if (!vehicle->returned &&
        vehicle->current_side == ferry.current_side &&
        vehicle->target_side == ferry.target_side &&
        ferry.load + vehicle->load <= FERRY_CAPACITY) {

        pthread_mutex_lock(&square_mutex[vehicle->current_side]);

        // Find and remove vehicle from square
        int found = 0;
        for (int i = 0; i < square_count[vehicle->current_side]; i++) {
            if (square[vehicle->current_side][i] == vehicle) {
                // Remove from square
                for (int j = i; j < square_count[vehicle->current_side] - 1; j++) {
                    square[vehicle->current_side][j] = square[vehicle->current_side][j + 1];
                }
                square[vehicle->current_side][square_count[vehicle->current_side] - 1] = NULL;
                square_count[vehicle->current_side]--;
                found = 1;

                // Add to ferry
                ferry_vehicles[ferry.vehicle_count++] = vehicle;
                ferry.load += vehicle->load;
                vehicle->on_ferry = 1;

                printf("The vehicle #%d got on the ferry. (Type of Vehicle: %s, Side: %c)\n",
                    vehicle->id,
                    vehicle->type == 1 ? "CAR" : vehicle->type == 2 ? "MINIBUS" : "TRUCK",
                    vehicle->current_side == 0 ? 'A' : 'B');
                break;
            }
        }

        pthread_mutex_unlock(&square_mutex[vehicle->current_side]);

        // If vehicle was found and boarded, wait a bit
        if (found) {
            usleep(50000);
        }
    }

    // Check if ferry should depart
    if (ferry.load == FERRY_CAPACITY ||
        (ferry.load >= 0 && !existsSuitableVehicleOnSide())) {
        departure_available_status = 1;
        pthread_cond_broadcast(&departure_available);
    }

    // Make ferry available for other vehicles
    ferry_available_status = 1;
    pthread_cond_broadcast(&ferry_available);
    pthread_mutex_unlock(&ferry_mutex);
}

void* ferry_departure() {
    while (!all_returned()) {
        pthread_mutex_lock(&ferry_mutex);

        // Check if ferry should depart
        if ((ferry.vehicle_count > 0) ||
            (ferry.vehicle_count == 0 && !existsSuitableVehicleOnSide() && existsSuitableVehicleOtherSide())) {

            // If ferry has vehicles, depart
            if (ferry.vehicle_count > 0) {
                printf("Ferry is departing from Side %c to Side %c with %d vehicles (Load: %d/%d)\n",
                    ferry.current_side == SIDE_A ? 'A' : 'B',
                    ferry.target_side == SIDE_A ? 'A' : 'B',
                    ferry.vehicle_count, ferry.load, FERRY_CAPACITY);
            }
            // If ferry is empty but should go to other side
            else {
                printf("Ferry is departing EMPTY from Side %c to Side %c\n",
                    ferry.current_side == SIDE_A ? 'A' : 'B',
                    ferry.target_side == SIDE_A ? 'A' : 'B');
            }

            // Prevent boarding during transit
            ferry_available_status = 0;

            // Move ferry to other side
            swap_ferry_side(&ferry);
            usleep(500000);

            // If ferry has vehicles, unload them
            if (ferry.vehicle_count > 0) {
                leaving_ferry();
            }

            // Allow boarding at new side
            ferry_available_status = 1;
            pthread_cond_broadcast(&ferry_available);
            pthread_mutex_unlock(&ferry_mutex);
            usleep(50000);
        }
        // If ferry shouldn't depart yet, wait
        else {
            // Wait for departure signal
            while (!departure_available_status && !all_returned()) {
                pthread_cond_wait(&departure_available, &ferry_mutex);
            }

            departure_available_status = 0;
            pthread_mutex_unlock(&ferry_mutex);
            usleep(100000);
        }

        // Check if simulation is complete
        if (all_returned()) {
            break;
        }
    }

    printf("All vehicles have returned. Ferry service ending.\n");
    return NULL;
}

void* vehicle_lifecycle(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;
    int in_square = 0; // Araç meydanda mı?

    while (!vehicle->returned) {
        if (!vehicle->on_ferry) {
            if (!in_square) {
                pass_toll(vehicle);
                wait_square(vehicle);
                in_square = 1;
            } else {
                boarding_ferry(vehicle);
                // boarding_ferry başarılı olursa, on_ferry 1 olacak
                // Eğer başarısız olursa, in_square değerini koruyoruz
            }
        } else {
            // Araç feribotta, in_square'i sıfırla ki
            // karşı tarafa vardığında yeniden gişeye girebilsin
            in_square = 0;
        }

        usleep(50000);
    }

    return NULL;
}