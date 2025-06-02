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
    double queue_entry_time;  // Time when vehicle entered the queue
    double boarding_time;     // Time when vehicle boarded the ferry
} Vehicle;

typedef struct {
    int load;
    int vehicle_count;
    int current_side;
    int target_side;
} Ferry;

// Trip information structure
typedef struct {
    int trip_number;
    int vehicle_count;
    int total_load;
    int remaining_capacity;
    Vehicle* vehicles[FERRY_CAPACITY];
    double trip_duration;
    int current_side;  // Side where the trip started
    int target_side;   // Side where the trip ended
} TripInfo;

extern TripInfo trip_history[100];

extern pthread_mutex_t toll_mutex[4]; // toll_mutex[0] and toll_mutex[1] for Side A, toll_mutex[2] and toll_mutex[3] for Side B
extern pthread_mutex_t square_mutex[2];
extern pthread_mutex_t ferry_mutex;

extern pthread_cond_t ferry_available;
extern pthread_cond_t departure_available;

extern Ferry ferry;
extern Vehicle* ferry_vehicles[FERRY_CAPACITY];

extern Vehicle* square[2][VEHICLE_COUNT];
extern int square_count[2];

extern pthread_t ferry_departure_thread;
extern pthread_t vehicleThreads[VEHICLE_COUNT];
extern Vehicle vehicles[VEHICLE_COUNT];

extern int ferry_available_status;
extern int departure_available_status;

const char* get_vehicle_type(int type);
double get_current_time();
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