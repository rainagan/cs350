#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
//#include <array.h>

/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */

#define NUM_DIRECTION 4

static struct cv *intersectionCV;
static struct lock *intersectionLock;
/* Heading from a direction to a direction
 * Direction representation:
 * 0 = north
 * 1 = south
 * 2 = west
 * 3 = east
 */
static int array_vehicle[NUM_DIRECTION][NUM_DIRECTION];

// static struct semaphore *intersectionSem;

// structure of a vehicle
/*typedef struct Vehicles
 {
 Direction origin;
 Direction destination;
 } Vehicle;
 
 // array of vehicles
 struct array *arr_vehicle;
 */
// forward declaration
//bool right_turn(Direction origin, Direction destination);
void enter_intersection(Direction origin, Direction destination);
void exit_intersection(Direction origin, Direction destination);
bool check_constraints(Direction origin, Direction destination);

/*
 * bool right_turn()
 *
 * Purpose:
 *   predicate that checks whether a vehicle is making a right turn
 *
 * Arguments:
 *   a pointer to a Vehicle
 *
 * Returns:
 *   true if the vehicle is making a right turn, else false
 *
 * Note: written this way to avoid a dependency on the specific
 *  assignment of numeric values to Directions
 */
/*bool
 right_turn(Direction origin, Direction destination) {
 if (((origin == west) && (destination == south)) ||
 ((origin == south) && (destination == east)) ||
 ((origin == east) && (destination == north)) ||
 ((origin == north) && (destination == west))) {
 return true;
 } else {
 return false;
 }
 }
 */

/*
 * check_constraints()
 *
 * Purpose:
 *   checks whether the entry of a vehicle into the intersection violates
 *   any synchronization constraints.
 *
 * Arguments:
 *   a pointer of a vehicle
 *
 * Returns:
 *   false if the vehicle meet the constraints, true otherwise
 */
void enter_intersection(Direction origin, Direction destination) {
    if (origin == north) {
        if (destination == south) {
            ++array_vehicle[1][2];  // s - w
            ++array_vehicle[2][0];  // w - n
            ++array_vehicle[2][1];  // w - s
            ++array_vehicle[2][3];  // w - e
            ++array_vehicle[3][1];  // e - s
            ++array_vehicle[3][2];  // e - w
        } else if (destination == west) { 
            ++array_vehicle[1][2];  // s - w
            ++array_vehicle[3][2];  // e - w
        } else if (destination == east) {
            ++array_vehicle[1][0];  // s - n
            ++array_vehicle[1][2];  // s - w
            ++array_vehicle[1][3];  // s - e
            ++array_vehicle[2][0];  // w - n
            ++array_vehicle[2][3];  // w - e
            ++array_vehicle[3][1];  // e - s
            ++array_vehicle[3][2];  // e - w
        } else {
            panic("invalid destination");
        }
    } else if (origin == south) {
        if (destination == north) {
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[2][0];  // w - n
            ++array_vehicle[2][3];  // w - e
            ++array_vehicle[3][0];  // e - n
            ++array_vehicle[3][1];  // e - s
            ++array_vehicle[3][2];  // e - w
        } else if (destination == west) {
            ++array_vehicle[0][1];  // n - s
            ++array_vehicle[0][2];  // n - w
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[2][0];  // w - n
            ++array_vehicle[2][3];  // w - e
            ++array_vehicle[3][1];  // e - s
            ++array_vehicle[3][2];  // e - w
        } else if (destination == east) {
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[2][3];  // w - e
        } else {
            panic("invalid destination");
        }
    } else if (origin == west) {
        if (destination == north) { 
            ++array_vehicle[0][1];  // n - s
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[1][0];  // s - n
            ++array_vehicle[1][2];  // s - w
            ++array_vehicle[3][0];  // e - n
            ++array_vehicle[3][1];  // e - s
            ++array_vehicle[3][2];  // e - w
        } else if (destination == south) { 
            ++array_vehicle[0][1];  // n - s
            ++array_vehicle[3][1];  // e - s
        } else if (destination == east) { 
            ++array_vehicle[0][1];  // n - s
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[1][0];  // s - n
            ++array_vehicle[1][2];  // s - w
            ++array_vehicle[1][3];  // s - e
            ++array_vehicle[3][1];  // e - s
        } else {
            panic("invalid destination");
        }
    } else if (origin == east) {
        if (destination == north) { 
            ++array_vehicle[1][0];  // s - n
            ++array_vehicle[2][0];  // w - n
        } else if (destination == south) {
            ++array_vehicle[0][1];  // n - s
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[2][0];  // w - n
            ++array_vehicle[2][1];  // w - s
            ++array_vehicle[2][3];  // w - e
            ++array_vehicle[3][0];  // s - n
            ++array_vehicle[3][2];  // s - w
        } else if (destination == west) {
            ++array_vehicle[0][1];  // n - s
            ++array_vehicle[0][2];  // n - w
            ++array_vehicle[0][3];  // n - e
            ++array_vehicle[1][0];  // s - n
            ++array_vehicle[1][2];  // s - w
            ++array_vehicle[2][0];  // w - n
        } else {
            panic("invalid destination");
        }
    } else {
        panic("invalid origin");
    }
}





/*
 * exit_intersection()
 *
 * Purpose:
 *   remove a vehicle that has exit the intersection and tell other vehicles that this vehicle has
 *   already leaved
 *
 * Arguments:
 *   two directions, origin and destination
 *
 * Returns:
 *   none
 */
void exit_intersection(Direction origin, Direction destination) {
    if (origin == north) {
        if (destination == south) {
            --array_vehicle[1][2];  // s - w
            --array_vehicle[2][0];  // w - n
            --array_vehicle[2][1];  // w - s
            --array_vehicle[2][3];  // w - e
            --array_vehicle[3][1];  // e - s
            --array_vehicle[3][2];  // e - w
        } else if (destination == west) { 
            --array_vehicle[1][2];  // s - w
            --array_vehicle[3][2];  // e - w
        } else if (destination == east) {
            --array_vehicle[1][0];  // s - n
            --array_vehicle[1][2];  // s - w
            --array_vehicle[1][3];  // s - e
            --array_vehicle[2][0];  // w - n
            --array_vehicle[2][3];  // w - e
            --array_vehicle[3][1];  // e - s
            --array_vehicle[3][2];  // e - w
        } else {
            panic("invalid destination");
        }
    } else if (origin == south) {
        if (destination == north) {
            --array_vehicle[0][3];  // n - e
            --array_vehicle[2][0];  // w - n
            --array_vehicle[2][3];  // w - e
            --array_vehicle[3][0];  // e - n
            --array_vehicle[3][1];  // e - s
            --array_vehicle[3][2];  // e - w
        } else if (destination == west) {
            --array_vehicle[0][1];  // n - s
            --array_vehicle[0][2];  // n - w
            --array_vehicle[0][3];  // n - e
            --array_vehicle[2][0];  // w - n
            --array_vehicle[2][3];  // w - e
            --array_vehicle[3][1];  // e - s
            --array_vehicle[3][2];  // e - w
        } else if (destination == east) {
            --array_vehicle[0][3];  // n - e
            --array_vehicle[2][3];  // w - e
        } else {
            panic("invalid destination");
        }
    } else if (origin == west) {
        if (destination == north) { 
            --array_vehicle[0][1];  // n - s
            --array_vehicle[0][3];  // n - e
            --array_vehicle[1][0];  // s - n
            --array_vehicle[1][2];  // s - w
            --array_vehicle[3][0];  // e - n
            --array_vehicle[3][1];  // e - s
            --array_vehicle[3][2];  // e - w
        } else if (destination == south) { 
            --array_vehicle[0][1];  // n - s
            --array_vehicle[3][1];  // e - s
        } else if (destination == east) { 
            --array_vehicle[0][1];  // n - s
            --array_vehicle[0][3];  // n - e
            --array_vehicle[1][0];  // s - n
            --array_vehicle[1][2];  // s - w
            --array_vehicle[1][3];  // s - e
            --array_vehicle[3][1];  // e - s
        } else {
            panic("invalid destination");
        }
    } else if (origin == east) {
        if (destination == north) { 
            --array_vehicle[1][0];  // s - n
            --array_vehicle[2][0];  // w - n
        } else if (destination == south) {
            --array_vehicle[0][1];  // n - s
            --array_vehicle[0][3];  // n - e
            --array_vehicle[2][0];  // w - n
            --array_vehicle[2][1];  // w - s
            --array_vehicle[2][3];  // w - e
            --array_vehicle[3][0];  // s - n
            --array_vehicle[3][2];  // s - w
        } else if (destination == west) {
            --array_vehicle[0][1];  // n - s
            --array_vehicle[0][2];  // n - w
            --array_vehicle[0][3];  // n - e
            --array_vehicle[1][0];  // s - n
            --array_vehicle[1][2];  // s - w
            --array_vehicle[2][0];  // w - n
        } else {
            panic("invalid destination");
        }
    } else {
        panic("invalid origin");
    }
}



bool check_constraints(Direction origin, Direction destination) {
    if (origin==north && destination==south) {
        return array_vehicle[0][1];
    } else if (origin==north && destination==west) {
        return array_vehicle[0][2];
    } else if (origin==north && destination==east) {
        return array_vehicle[0][3];
    } else if (origin==south && destination==north) {
        return array_vehicle[1][0];
    }  else if (origin==south && destination==west) {
        return array_vehicle[1][2];
    }  else if (origin==south && destination==east) {
        return array_vehicle[1][3];
    }  else if (origin==west && destination==north) {
        return array_vehicle[2][0];
    }  else if (origin==west && destination==south) {
        return array_vehicle[2][1];
    }  else if (origin==west && destination==east) {
        return array_vehicle[2][3];
    }  else if (origin==east && destination==north) {
        return array_vehicle[3][0];
    }  else if (origin==east && destination==south) {
        return array_vehicle[3][1];
    }  else {
        return array_vehicle[3][2];
    }
}


/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */
void
intersection_sync_init(void)
{
    /* replace this default implementation with your own implementation */
    
    //  intersectionSem = sem_create("intersectionSem",1);
    intersectionCV = cv_create("intersectionCV");
    intersectionLock = lock_create("intersectionLock");
    
    //arr_vehicle = array_create();
    //array_init(arr_vehicle);
    
    if (intersectionCV == NULL) {
        panic("could not create intersection cv");
    }
    if (intersectionLock == NULL) {
        panic("could not create intersection lock");
    }
    
    for (int i=0; i<NUM_DIRECTION;++i) {
        for (int j=0; j<NUM_DIRECTION;++j) {
            array_vehicle[i][j] = 0;
        }
    }
    return;
}

/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
    /* replace this default implementation with your own implementation */
    KASSERT(intersectionCV != NULL);
    KASSERT(intersectionLock != NULL);
    
    cv_destroy(intersectionCV);
    lock_destroy(intersectionLock);
    
    return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination)
{
    /* replace this default implementation with your own implementation */
    //(void)origin;  /* avoid compiler complaint about unused parameter */
    //(void)destination; /* avoid compiler complaint about unused parameter */
    KASSERT(intersectionCV != NULL);
    KASSERT(intersectionLock != NULL);
    
    lock_acquire(intersectionLock);
    
    while(check_constraints(origin,destination)){
        cv_wait(intersectionCV,intersectionLock);
    };
    //array_add(arr_vehicle,v,NULL);     // add v to arr_vehicle without return its index
    enter_intersection(origin,destination);
    
    lock_release(intersectionLock);
    
    return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination)
{
    /* replace this default implementation with your own implementation */
    // (void)origin;  /* avoid compiler complaint about unused parameter */
    //(void)destination; /* avoid compiler complaint about unused parameter */
    KASSERT(intersectionCV != NULL);
    KASSERT(intersectionLock != NULL);
    
    lock_acquire(intersectionLock);
    exit_intersection(origin,destination);
    cv_broadcast(intersectionCV,intersectionLock);
    lock_release(intersectionLock);
    
    return;
}

