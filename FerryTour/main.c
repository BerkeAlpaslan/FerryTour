#include "ferry.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

/*
 *
 *
 * 210316016 - Berke Alpaslan - Night Education
 *
 *
*/

int main() {
    srand(time(NULL));

    pthread_create(&ferry_departure_thread, NULL, (void*)ferry_departure, NULL);

    init_variables();

    for (int i = 0; i < 4; i++) {
        pthread_mutex_init(&toll_mutex[i], NULL);
    }

    for (int i = 0; i < 2; i++) {
        pthread_mutex_init(&square_mutex[i], NULL);
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        pthread_create(&vehicleThreads[i], NULL, vehicle_lifecycle, &vehicles[i]);
    }

    pthread_join(ferry_departure_thread, NULL);

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
    pthread_cond_destroy(&ferry_available);
    pthread_cond_destroy(&departure_available);

    printf("\nAll vehicles have returned to their initial side. Simulation finished.\n");
    return 0;
}