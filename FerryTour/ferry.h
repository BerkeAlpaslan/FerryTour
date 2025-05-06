#ifndef FERRY_H
#define FERRY_H

#include<pthread.h>

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
    int initial_side;
    int current_side;
    int targetSide;
    int returned;
} Vehicle;

extern pthread_mutex_t toll_mutex[4];
extern pthread_mutex_t square_mutex[2];
extern pthread_mutex_t ferry_mutex;
extern pthread_cond_t ferry_full;
extern pthread_cond_t ferry_empty;

extern Vehicle* ferry[FERRY_CAPACITY];
extern int ferryLoad;
extern int ferryVehicleCount;
extern int ferrySide;

extern Vehicle* square[2][VEHICLE_COUNT];
extern int squareCount[2];

extern Vehicle vehicles[VEHICLE_COUNT];
extern pthread_t vehicleThreads[VEHICLE_COUNT];

extern int finished;

void init_vehicles();
void wait_random();
int existsSuitableVehicleOnSide();
int existsSuitableVehicleOtherSide();
int all_returned();
void pass_toll(Vehicle* vehicle);
void wait_random();
void wait_square(Vehicle* vehicle);
void* boarding_ferry(void* arg);
void* ferry_departure();
void* leaving_ferry(Vehicle* vehicle);

#endif