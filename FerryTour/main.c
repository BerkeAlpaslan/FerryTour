#include "ferry.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

void* vehicle_lifecycle(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;

    pass_toll(vehicle);
    wait_square(vehicle);

    while (!vehicle->returned) {
        boarding_ferry(vehicle);
        ferry_departure();
        wait_square(vehicle);
        pass_toll(vehicle);
        wait_square(vehicle);
    }

    return NULL;
}

int main() {
    srand(time(NULL));
    init_vehicles();

    for (int i = 0; i < 4; i++) {
        pthread_mutex_init(&toll_mutex[i], NULL);
    }

    for (int i = 0; i < 2; i++) {
        pthread_mutex_init(&square_mutex[i], NULL);
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        pthread_create(&vehicleThreads[i], NULL, vehicle_lifecycle, &vehicles[i]);
    }

    while (!all_returned()) {
        ferry_departure();
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        pthread_join(vehicleThreads[i], NULL);
    }

    for (int i = 0; i < 4; i++) {
        pthread_mutex_destroy(&toll_mutex[i]);
    }

    for (int i = 0; i < 2; i++) {
        pthread_mutex_destroy(&square_mutex[i]);
    }

    pthread_mutex_destroy(&ferry_mutex);
    pthread_cond_destroy(&ferry_full);
    pthread_cond_destroy(&ferry_empty);

    printf("\nAll vehicles have returned to their initial side. Simulation finished.\n");
    return 0;
}