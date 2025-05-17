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
    int target_side;
    int on_ferry;
    int returned;
} Vehicle;

typedef struct {
    int load;
    int vehicle_count;
    int current_side;
    int target_side;
} Ferry;

typedef struct {
    Vehicle* vehicle;
    struct Node* next;
} Node;

extern pthread_mutex_t toll_mutex[4];
extern pthread_mutex_t square_mutex[2];
extern pthread_mutex_t ferry_mutex;

extern pthread_cond_t ferry_available;
extern pthread_cond_t departure_available;

extern Ferry ferry;
extern Vehicle* ferry_vehicles[FERRY_CAPACITY];

extern Vehicle* square[2][VEHICLE_COUNT];
extern int squareCount[2];

extern pthread_t ferry_departure_thread;
extern pthread_t vehicleThreads[VEHICLE_COUNT];
extern Vehicle vehicles[VEHICLE_COUNT];

extern int ferry_available_status;
extern int departure_available_status;

void init_variables();
int existsSuitableVehicleOnSide();
int existsSuitableVehicleOtherSide();
int all_returned();
void swap_vehicle_side(Vehicle* vehicle);
void swap_ferry_side(Ferry* ferry);
void pass_toll(Vehicle* vehicle);
void wait_square(Vehicle* vehicle);
void boarding_ferry(void* arg);
void leaving_ferry();
void* ferry_departure();
void* vehicle_lifecycle(void* arg);

#endif