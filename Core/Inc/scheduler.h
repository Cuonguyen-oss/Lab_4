
#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include <stdint.h>

/* Scheduler configuration */
#define SCH_MAX_TASKS       16U
#define SCH_TIMERTICK       10U

/* Error value when task registration fails */
#define SCH_TASK_ERROR      UINT32_MAX

typedef struct
{
    void (*functionPointer)(void);

    uint32_t id;

    /* Absolute execution time in milliseconds */
    uint32_t deadline;

    /* Zero = one-shot, non-zero = periodic */
    uint32_t period;

} SCH_Task;

/* Scheduler functions */
void SCH_Init(void);

void SCH_Update(void);

void SCH_Dispatch_Tasks(void);

uint32_t SCH_Add_Task(
    void (*pFunction)(void),
    uint32_t DELAY,
    uint32_t PERIOD
);

uint8_t SCH_Delete_Task(uint32_t taskID);

/* Return scheduler time in milliseconds */
uint32_t get_time(void);

#endif /* INC_SCHEDULER_H_ */
