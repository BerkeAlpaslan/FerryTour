#include "ferry.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

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

// Global variables for statistics
int total_trips = 0;
int total_vehicles_transported = 0;
double total_waiting_time = 0.0;
double simulation_start_time = 0.0;
int departure_triggered = 0;  // Flag to prevent multiple departure messages

// Trip information structure
TripInfo trip_history[100];

// Helper function to get vehicle type as string
const char* get_vehicle_type(int type) {
    switch(type) {
        case CAR: return "CAR";
        case MINIBUS: return "MINIBUS";
        case TRUCK: return "TRUCK";
        default: return "UNKNOWN";
    }
}

// Get current time in seconds
double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void init_variables() {
    int i;
    int j = 0;
    int side = rand() % 2;

    // Init ferry
    ferry = (Ferry) {0, 0, side, !side};
    printf("[FERRY] Initialized (Current Side: %c, Target Side: %c)\n",
        ferry.current_side == SIDE_A ? 'A' : 'B',
        ferry.target_side == SIDE_A ? 'A' : 'B');

    // Init 12 cars
    for (i = 0; i < 12; i++) {
        side = rand() % 2;
        vehicles[j] = (Vehicle) {j, CAR, 1, side, side, side == 0 ? SIDE_B : SIDE_A, 0, 0};
        vehicles[j].queue_entry_time = 0.0;
        vehicles[j].boarding_time = 0.0;
        printf("[VEHICLE #%2d][%s] Created (Side: %c)\n",
            vehicles[j].id + 1, get_vehicle_type(vehicles[j].type),
            vehicles[j].initial_side == SIDE_A ? 'A' : 'B');
        j++;
    }

    // Init 10 minibuses
    for (i = 0; i < 10; i++) {
        side = rand() % 2;
        vehicles[j] = (Vehicle) {j, MINIBUS, 2, side, side, side == 0 ? SIDE_B : SIDE_A, 0, 0};
        vehicles[j].queue_entry_time = 0.0;
        vehicles[j].boarding_time = 0.0;
        printf("[VEHICLE #%2d][%s] Created (Side: %c)\n",
            vehicles[j].id + 1, get_vehicle_type(vehicles[j].type),
            vehicles[j].initial_side == SIDE_A ? 'A' : 'B');
        j++;
    }

    // Init 8 trucks
    for (i = 0; i < 8; i++) {
        side = rand() % 2;
        vehicles[j] = (Vehicle) {j, TRUCK, 3, side, side, side == 0 ? SIDE_B : SIDE_A, 0, 0};
        vehicles[j].queue_entry_time = 0.0;
        vehicles[j].boarding_time = 0.0;
        printf("[VEHICLE #%2d][%s] Created (Side: %c)\n",
            vehicles[j].id + 1, get_vehicle_type(vehicles[j].type),
            vehicles[j].initial_side == SIDE_A ? 'A' : 'B');
        j++;
    }

    // Set simulation start time
    simulation_start_time = get_current_time();
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
            printf("[VEHICLE #%2d][%s] Has returned to initial side (Side: %c)\n",
                vehicle->id + 1, get_vehicle_type(vehicle->type),
                vehicle->initial_side == SIDE_A ? 'A' : 'B');
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

    printf("[VEHICLE #%2d][%s] Passing through toll (Side: %c, Toll: %c-%d)\n",
        vehicle->id + 1, get_vehicle_type(vehicle->type),
        vehicle->current_side == 0 ? 'A' : 'B', vehicle->current_side == 0 ? 'A' : 'B', toll_index + 1);

    usleep(10000);

    pthread_mutex_unlock(&toll_mutex[vehicle->current_side * 2 + toll_index]);
}

void wait_square(Vehicle* vehicle) {
    pthread_mutex_lock(&square_mutex[vehicle->current_side]);

    vehicle->queue_entry_time = get_current_time();
    square[vehicle->current_side][square_count[vehicle->current_side]++] = vehicle;
    printf("[VEHICLE #%2d][%s] Waiting in square (Side: %c, Square Queue Length: %d)\n",
        vehicle->id + 1, get_vehicle_type(vehicle->type),
        vehicle->current_side == SIDE_A ? 'A' : 'B',
        square_count[vehicle->current_side]);
    usleep(10000);

    pthread_mutex_unlock(&square_mutex[vehicle->current_side]);
}

void leaving_ferry() {
    printf("\n--- UNLOADING VEHICLES ---\n");
    for (int i = 0; i < ferry.vehicle_count; i++) {
        Vehicle* vehicle = ferry_vehicles[i];
        if (vehicle != NULL) {
            double ride_time = get_current_time() - vehicle->boarding_time;
            printf("[VEHICLE #%2d][%s] Got off from ferry (Side: %c, Ferry Travel Time: %.3f sec)\n",
                vehicle->id + 1, get_vehicle_type(vehicle->type),
                ferry.current_side == SIDE_A ? 'A' : 'B',
                ride_time);

            swap_vehicle_side(vehicle);
            vehicle->on_ferry = 0;
            ferry_vehicles[i] = NULL;
            total_vehicles_transported++;
        }
    }
    printf("--- UNLOADING COMPLETE ---\n\n");

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
        !vehicle->on_ferry &&
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
                vehicle->boarding_time = get_current_time();

                double waiting_time = vehicle->boarding_time - vehicle->queue_entry_time;
                total_waiting_time += waiting_time;

                printf("[VEHICLE #%2d][%s] Boarded ferry (Side: %c, Waiting Time: %.3f sec, Ferry Load: %d/%d)\n",
                    vehicle->id + 1, get_vehicle_type(vehicle->type),
                    vehicle->current_side == 0 ? 'A' : 'B',
                    waiting_time, ferry.load, FERRY_CAPACITY);
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
        (ferry.load > 0 && !existsSuitableVehicleOnSide())) {

        // Check departure reason
        if (!departure_triggered) {
            if (ferry.load == FERRY_CAPACITY) {
                // Case 1: Ferry is full
                printf("\n[FERRY] Ferry is full (20/20) → Departure triggered\n");
                departure_triggered = 1;
            } else {
                // Check if there's a vehicle that cannot fit
                int found_unfit = 0;
                pthread_mutex_lock(&square_mutex[ferry.current_side]);
                for (int i = 0; i < square_count[ferry.current_side]; i++) {
                    if (square[ferry.current_side][i] != NULL &&
                        square[ferry.current_side][i]->target_side == ferry.target_side &&
                        square[ferry.current_side][i]->load + ferry.load > FERRY_CAPACITY &&
                        !square[ferry.current_side][i]->returned &&
                        !square[ferry.current_side][i]->on_ferry) {
                        // Case 2: Vehicle cannot fit
                        printf("\n[FERRY] Vehicle #%d (%s) cannot fit: %d + %d = %d > %d (Ferry Capacity) → Departure triggered\n",
                            square[ferry.current_side][i]->id + 1,
                            get_vehicle_type(square[ferry.current_side][i]->type),
                            ferry.load,
                            square[ferry.current_side][i]->load,
                            ferry.load + square[ferry.current_side][i]->load,
                            FERRY_CAPACITY);
                        departure_triggered = 1;
                        found_unfit = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&square_mutex[ferry.current_side]);

                // Case 3: No more suitable vehicles
                if (!found_unfit && !existsSuitableVehicleOnSide()) {
                    printf("\n[FERRY] All suitable vehicles in square have boarded → Departure triggered\n");
                    departure_triggered = 1;
                }
            }
        }

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
        usleep(1000000);
        pthread_mutex_lock(&ferry_mutex);

        // Check if ferry should depart
        if ((ferry.vehicle_count > 0) ||
            (ferry.vehicle_count == 0 && !existsSuitableVehicleOnSide() && existsSuitableVehicleOtherSide())) {

            total_trips++;
            double trip_start_time = get_current_time();

            // Store trip info
            trip_history[total_trips-1].trip_number = total_trips;
            trip_history[total_trips-1].vehicle_count = ferry.vehicle_count;
            trip_history[total_trips-1].total_load = ferry.load;
            trip_history[total_trips-1].remaining_capacity = FERRY_CAPACITY - ferry.load;
            trip_history[total_trips-1].current_side = ferry.current_side;
            trip_history[total_trips-1].target_side = ferry.target_side;

            // If ferry has vehicles, depart
            if (ferry.vehicle_count > 0) {
                printf("\n===============================================\n");
                printf("[FERRY] TRIP %d DEPARTING!\n", total_trips);
                printf("  From Side: %c → To Side: %c\n",
                    ferry.current_side == SIDE_A ? 'A' : 'B',
                    ferry.target_side == SIDE_A ? 'A' : 'B');
                printf("  Vehicles on board: %d\n", ferry.vehicle_count);
                printf("  Total load: %d/%d units\n", ferry.load, FERRY_CAPACITY);
                printf("  Remaining capacity: %d units\n", FERRY_CAPACITY - ferry.load);
                printf("\n  VEHICLES ON BOARD:\n");

                for (int i = 0; i < ferry.vehicle_count; i++) {
                    printf("    %d. Vehicle #%2d (%s) - %d units\n",
                        i+1, ferry_vehicles[i]->id + 1,
                        get_vehicle_type(ferry_vehicles[i]->type),
                        ferry_vehicles[i]->load);
                    trip_history[total_trips-1].vehicles[i] = ferry_vehicles[i];
                }
                printf("===============================================\n");
            }
            // If ferry is empty but should go to other side
            else {
                printf("\n[FERRY] Trip %d - Departing EMPTY from Side %c to Side %c\n",
                    total_trips,
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

            double trip_end_time = get_current_time();
            trip_history[total_trips-1].trip_duration = trip_end_time - trip_start_time;
            printf("[FERRY] Trip %d completed (Duration: %.3f sec)\n\n",
                total_trips, trip_history[total_trips-1].trip_duration);
            printf("===============================================\n");

            // Reset departure trigger flag
            departure_triggered = 0;

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
            usleep(1000000);
        }

        // Check if simulation is complete
        if (all_returned()) {
            break;
        }
    }

    printf("\n[FERRY] All vehicles have returned. Ferry service ending.\n");

    // Print summary
    double total_time = get_current_time() - simulation_start_time;
    printf("\n==================== SIMULATION SUMMARY ====================\n");
    printf("Total trips: %d\n", total_trips);
    printf("Total vehicles transported: %d\n", total_vehicles_transported);
    printf("Total simulation time: %.3f seconds\n", total_time);
    printf("Total waiting time in queue: %.3f seconds\n", total_waiting_time);
    printf("Average waiting time per vehicle: %.3f seconds\n",
        total_waiting_time / total_vehicles_transported);

    printf("\n--- TRIP DETAILS ---\n");
    for (int i = 0; i < total_trips; i++) {
        printf("Trip %d: From %s → To %s, %d vehicles, %d/%d units of Ferry Capacity used, %d units of Ferry Capacity free, Duration: %.3f sec\n",
            trip_history[i].trip_number,
            trip_history[i].current_side == SIDE_A ? "SIDE A" : "SIDE B",
            trip_history[i].target_side == SIDE_A ? "SIDE A" : "SIDE B",
            trip_history[i].vehicle_count,
            trip_history[i].total_load,
            FERRY_CAPACITY,
            trip_history[i].remaining_capacity,
            trip_history[i].trip_duration);
        printf("  Vehicles: ");
        for (int j = 0; j < trip_history[i].vehicle_count; j++) {
            printf("#%d(%s) ",
                trip_history[i].vehicles[j]->id + 1,
                get_vehicle_type(trip_history[i].vehicles[j]->type));
            if (j == trip_history[i].vehicle_count - 1) {
                printf("\n");
            }
        }
        printf("\n");
    }
    printf("==========================================================\n");

    return NULL;
}

void* vehicle_lifecycle(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;
    int in_square = 0;

    while (!vehicle->returned) {
        if (!vehicle->on_ferry) {
            if (!in_square) {
                pass_toll(vehicle);
                wait_square(vehicle);
                in_square = 1;
            } else {
                boarding_ferry(vehicle);
            }
        } else {
            in_square = 0;
        }

        usleep(50000);
    }

    return NULL;
}