#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_random.h"

// Define a structure for car arrival events passed via queue
typedef struct {
    int lane;       // 1 for Lane A, 2 for Lane B
    int count;      // Number of cars arriving
} car_event_t;

// Shared lane states and queue handle
static int lane_a_cars = 0;
static int lane_b_cars = 0;
static int active_lane = 0; // 0 = None, 1 = Lane A, 2 = Lane B
static QueueHandle_t car_queue = NULL;

// Task 1: Generator Task (Pushes random car arrivals 1-10 every 5 seconds)
void generator_task(void *pvParameters) {
    car_event_t event;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Generate for Lane A
        event.lane = 1;
        event.count = (esp_random() % 10) + 1;
        xQueueSend(car_queue, &event, portMAX_DELAY);

        // Generate for Lane B
        event.lane = 2;
        event.count = (esp_random() % 10) + 1;
        xQueueSend(car_queue, &event, portMAX_DELAY);
        
        printf("[Generator] Generated arrivals sent to queue.\n");
    }
}

// Task 2: Queue Processor & Scheduler Task (Processes queue data, handles priority and departures)
void scheduler_and_traffic_task(void *pvParameters) {
    car_event_t received_event;
    TickType_t last_departure_tick = xTaskGetTickCount();
    TickType_t last_display_tick = xTaskGetTickCount();

    while (1) {
        // Non-blocking or short-timeout check on queue for new car arrivals
        if (xQueueReceive(car_queue, &received_event, 0) == pdPASS) {
            if (received_event.lane == 1) {
                lane_a_cars += received_event.count;
                printf("[Queue] Added %d cars to Lane A. Total: %d\n", received_event.count, lane_a_cars);
            } else if (received_event.lane == 2) {
                lane_b_cars += received_event.count;
                printf("[Queue] Added %d cars to Lane B. Total: %d\n", received_event.count, lane_b_cars);
            }
        }

        // Determine priority if no active lane or current lane is empty
        if (active_lane == 0) {
            if (lane_a_cars >= lane_b_cars && lane_a_cars > 0) {
                active_lane = 1;
                printf("[Scheduler] Priority given to Lane A (%d cars waiting)\n", lane_a_cars);
            } else if (lane_b_cars > lane_a_cars && lane_b_cars > 0) {
                active_lane = 2;
                printf("[Scheduler] Priority given to Lane B (%d cars waiting)\n", lane_b_cars);
            }
        }

        // Every 1 second: a vehicle leaves from the selected lane
        if ((xTaskGetTickCount() - last_departure_tick) >= pdMS_TO_TICKS(1000)) {
            last_departure_tick = xTaskGetTickCount();
            
            if (active_lane == 1 && lane_a_cars > 0) {
                lane_a_cars--;
                printf("[Traffic] 1 vehicle left Lane A. Remaining: %d\n", lane_a_cars);
                if (lane_a_cars == 0) active_lane = 0;
            } else if (active_lane == 2 && lane_b_cars > 0) {
                lane_b_cars--;
                printf("[Traffic] 1 vehicle left Lane B. Remaining: %d\n", lane_b_cars);
                if (lane_b_cars == 0) active_lane = 0;
            }
        }

        // Every 4 seconds: display status of both lanes
        if ((xTaskGetTickCount() - last_display_tick) >= pdMS_TO_TICKS(4000)) {
            last_display_tick = xTaskGetTickCount();
            printf("========================================\n");
            printf("[Display] Lane A: %d cars | Lane B: %d cars | Active: %s\n", 
                   lane_a_cars, lane_b_cars, active_lane == 1 ? "Lane A" : (active_lane == 2 ? "Lane B" : "None"));
            printf("========================================\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Small yield to prevent CPU starvation
    }
}

void app_main(void) {
    // Create a FreeRTOS queue capable of holding 10 car events
    car_queue = xQueueCreate(10, sizeof(car_event_t));
    
    if (car_queue != NULL) {
        xTaskCreate(generator_task, "generator_task", 2048, NULL, 2, NULL);
        xTaskCreate(scheduler_and_traffic_task, "scheduler_task", 4096, NULL, 2, NULL);
    }
}