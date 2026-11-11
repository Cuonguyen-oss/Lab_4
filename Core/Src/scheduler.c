
#include "scheduler.h"
#include "main.h"

#include <stddef.h>

/* =====================================================
 * SCHEDULER DATA
 * ===================================================== */

static SCH_Task tasks[SCH_MAX_TASKS];

static uint32_t task_count = 0;

/* Scheduler clock in milliseconds */
static volatile uint32_t scheduler_time = 0;


/* =====================================================
 * CRITICAL SECTION
 * ===================================================== */

static uint32_t SCH_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    return primask;
}

static void SCH_ExitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
}


/* =====================================================
 * TIME COMPARISON
 *
 * Supports uint32_t clock rollover.
 * All scheduled delays must be less than 2^31 ms.
 * ===================================================== */

static uint8_t SCH_TimeReached(
    uint32_t now,
    uint32_t deadline
)
{
    return ((int32_t)(now - deadline) >= 0);
}


/* =====================================================
 * TASK ORDERING
 *
 * Sort by absolute deadline.
 * Tasks with the same deadline are sorted by ID.
 * ===================================================== */

static uint8_t SCH_TaskComesBefore(
    const SCH_Task *a,
    const SCH_Task *b
)
{
    int32_t difference =
        (int32_t)(a->deadline - b->deadline);

    if (difference < 0)
    {
        return 1;
    }

    if (difference > 0)
    {
        return 0;
    }

    return (a->id < b->id);
}


/* =====================================================
 * FIND FREE TASK ID
 *
 * O(n), used only when adding a task.
 * ===================================================== */

static uint32_t SCH_FindFreeIDLocked(void)
{
    for (uint32_t candidate = 0;
         candidate < SCH_MAX_TASKS;
         candidate++)
    {
        uint8_t used = 0;

        for (uint32_t i = 0; i < task_count; i++)
        {
            if (tasks[i].id == candidate)
            {
                used = 1;
                break;
            }
        }

        if (!used)
        {
            return candidate;
        }
    }

    return SCH_TASK_ERROR;
}


/* =====================================================
 * INSERT TASK
 *
 * Maintain an ordered absolute-deadline queue.
 * Must be called with interrupts disabled.
 * ===================================================== */

static uint8_t SCH_InsertTaskLocked(SCH_Task newTask)
{
    if (newTask.functionPointer == NULL ||
        task_count >= SCH_MAX_TASKS)
    {
        return 0;
    }

    uint32_t index = 0;

    while (index < task_count &&
           !SCH_TaskComesBefore(&newTask, &tasks[index]))
    {
        index++;
    }

    /* Shift tasks to create an empty slot */
    for (uint32_t j = task_count; j > index; j--)
    {
        tasks[j] = tasks[j - 1];
    }

    tasks[index] = newTask;

    task_count++;

    return 1;
}


/* =====================================================
 * REMOVE TASK AT INDEX
 *
 * Must be called with interrupts disabled.
 * ===================================================== */

static SCH_Task SCH_RemoveTaskAtLocked(uint32_t index)
{
    SCH_Task removedTask = tasks[index];

    for (uint32_t i = index; i + 1 < task_count; i++)
    {
        tasks[i] = tasks[i + 1];
    }

    task_count--;

    tasks[task_count].functionPointer = NULL;
    tasks[task_count].id = 0;
    tasks[task_count].deadline = 0;
    tasks[task_count].period = 0;

    return removedTask;
}


/* =====================================================
 * SCH INIT
 * ===================================================== */

void SCH_Init(void)
{
    uint32_t primask = SCH_EnterCritical();

    scheduler_time = 0;

    task_count = 0;

    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++)
    {
        tasks[i].functionPointer = NULL;
        tasks[i].id = 0;
        tasks[i].deadline = 0;
        tasks[i].period = 0;
    }

    SCH_ExitCritical(primask);
}


/* =====================================================
 * SCH UPDATE
 *
 * Called every 10 ms from TIM2 ISR.
 *
 * O(1): Only update the scheduler clock.
 * No task searching or task execution here.
 * ===================================================== */

void SCH_Update(void)
{
    scheduler_time += SCH_TIMERTICK;
}


/* =====================================================
 * SCH ADD TASK
 *
 * DELAY is relative to current scheduler time.
 * PERIOD = 0 means one-shot.
 * ===================================================== */

uint32_t SCH_Add_Task(
    void (*pFunction)(void),
    uint32_t DELAY,
    uint32_t PERIOD
)
{
    if (pFunction == NULL)
    {
        return SCH_TASK_ERROR;
    }

    uint32_t primask = SCH_EnterCritical();

    if (task_count >= SCH_MAX_TASKS)
    {
        SCH_ExitCritical(primask);
        return SCH_TASK_ERROR;
    }

    uint32_t newID = SCH_FindFreeIDLocked();

    if (newID == SCH_TASK_ERROR)
    {
        SCH_ExitCritical(primask);
        return SCH_TASK_ERROR;
    }

    SCH_Task newTask;

    newTask.functionPointer = pFunction;
    newTask.id = newID;
    newTask.deadline = scheduler_time + DELAY;
    newTask.period = PERIOD;

    uint8_t success = SCH_InsertTaskLocked(newTask);

    SCH_ExitCritical(primask);

    if (!success)
    {
        return SCH_TASK_ERROR;
    }

    return newID;
}


/* =====================================================
 * SCH DELETE TASK
 *
 * Delete a task using its ID.
 * ===================================================== */

uint8_t SCH_Delete_Task(uint32_t taskID)
{
    uint32_t primask = SCH_EnterCritical();

    for (uint32_t i = 0; i < task_count; i++)
    {
        if (tasks[i].id == taskID)
        {
            SCH_RemoveTaskAtLocked(i);

            SCH_ExitCritical(primask);

            return 1;
        }
    }

    SCH_ExitCritical(primask);

    return 0;
}


/* =====================================================
 * SCH DISPATCH TASKS
 *
 * Execute all tasks whose deadlines have arrived.
 *
 * Periodic tasks are rescheduled from their previous
 * absolute deadline, preventing cumulative drift.
 *
 * Task callbacks execute outside critical sections.
 * ===================================================== */

void SCH_Dispatch_Tasks(void)
{
    while (1)
    {
        SCH_Task currentTask;

        uint8_t taskReady = 0;

        uint32_t primask = SCH_EnterCritical();

        uint32_t now = scheduler_time;

        /* Check the earliest task */
        if (task_count > 0 &&
            SCH_TimeReached(now, tasks[0].deadline))
        {
            /* Remove the earliest task */
            currentTask = SCH_RemoveTaskAtLocked(0);

            taskReady = 1;

            /* Reschedule periodic task */
            if (currentTask.period > 0)
            {
                SCH_Task nextTask = currentTask;

                /*
                 * Calculate the next deadline from
                 * the previous scheduled deadline.
                 */
                uint32_t nextDeadline =
                    currentTask.deadline + currentTask.period;

                /*
                 * Skip missed periods if the dispatcher
                 * was delayed. Do not execute tasks in
                 * a burst to catch up.
                 */
                if (SCH_TimeReached(now, nextDeadline))
                {
                    uint32_t lateTime =
                        now - nextDeadline;

                    uint32_t periodsToSkip =
                        (lateTime / currentTask.period) + 1U;

                    nextDeadline +=
                        periodsToSkip * currentTask.period;
                }

                nextTask.deadline = nextDeadline;

                /*
                 * One task was removed, so there is
                 * always a free slot for rescheduling.
                 */
                SCH_InsertTaskLocked(nextTask);
            }
        }

        SCH_ExitCritical(primask);

        /* No task is ready */
        if (!taskReady)
        {
            break;
        }

        /* Execute outside critical section */
        currentTask.functionPointer();
    }
}


/* =====================================================
 * GET TIME
 *
 * Return scheduler time in milliseconds.
 * ===================================================== */

uint32_t get_time(void)
{
    uint32_t primask = SCH_EnterCritical();

    uint32_t currentTime = scheduler_time;

    SCH_ExitCritical(primask);

    return currentTime;
}
