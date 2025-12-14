#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 10
#define INF 999999

typedef struct {
    char pid[3];
    int arrival_time;
    int burst_time;
    int remaining_time;
    int start_time;
    int finish_time;
    int waiting_time;
    int turnaround_time;
} Process;

typedef struct {
    int data[MAX_PROCESSES];
    int front, rear;
} Queue;

void enqueue(Queue *q, int index) {
    if (q->rear < MAX_PROCESSES) {
        q->data[q->rear++] = index;
    }
}

int dequeue(Queue *q) {
    if (q->front < q->rear) {
        return q->data[q->front++];
    }
    return -1;
}
int compare_arrival(const void *a, const void *b) {
    return ((Process *)a)->arrival_time - ((Process *)b)->arrival_time;
}

void calculate_and_print_metrics(Process processes[], int count) {
    float total_waiting = 0;
    float total_turnaround = 0;

    printf("\n| Process | Arrival | Burst | Start | Finish | Waiting | Turnaround |\n");
    printf("|---------|---------|-------|-------|--------|---------|------------|\n");

    for (int i = 0; i < count; i++) {
        processes[i].waiting_time = processes[i].start_time - processes[i].arrival_time;
        processes[i].turnaround_time = processes[i].finish_time - processes[i].arrival_time;

        total_waiting += processes[i].waiting_time;
        total_turnaround += processes[i].turnaround_time;

        printf("| %-7s | %-7d | %-5d | %-5d | %-6d | %-7d | %-10d |\n", processes[i].pid, processes[i].arrival_time, processes[i].burst_time, processes[i].start_time, processes[i].finish_time, processes[i].waiting_time, processes[i].turnaround_time);
    }

    printf("\nAverage Waiting Time: %.2f\n", total_waiting / count);
    printf("Average Turnaround Time: %.2f\n", total_turnaround / count);
}

void print_gantt_chart(const char *gantt_chart) {
    printf("\nGantt Chart:\n");
    printf("%s\n", gantt_chart);
}

void initialize_processes(Process processes[], const Process template[], int count) {
    for (int i = 0; i < count; i++) {
        processes[i] = template[i];
        processes[i].remaining_time = template[i].burst_time;
        processes[i].start_time = -1;
        processes[i].finish_time = -1;
    }
}

void run_fcfs(Process processes[], int count) {
    Process sorted_processes[MAX_PROCESSES];
    initialize_processes(sorted_processes, processes, count);
    qsort(sorted_processes, count, sizeof(Process), compare_arrival);
    int current_time = 0;
    char gantt_chart[512] = "";
    int offset = 0;
    printf("\nFCFS\n");

    for (int i = 0; i < count; i++) {
        Process *p = &sorted_processes[i];

        if (current_time < p->arrival_time) {
            current_time = p->arrival_time;
        }

        p->start_time = current_time;
        current_time += p->burst_time;
        p->finish_time = current_time;

        offset += snprintf(gantt_chart + offset, sizeof(gantt_chart) - offset, "| %s ", p->pid);
    }
    offset += snprintf(gantt_chart + offset, sizeof(gantt_chart) - offset, "|");
    print_gantt_chart(gantt_chart);
    calculate_and_print_metrics(sorted_processes, count);
}

void run_sjf(Process processes[], int count) {
    Process sjf_processes[MAX_PROCESSES];
    initialize_processes(sjf_processes, processes, count);

    int current_time = 0;
    int completed = 0;
    int min_burst;
    int shortest_job_index;
    int execution_order[MAX_PROCESSES];
    int order_index = 0;
    char gantt_chart[512] = "";
    int offset = 0;

    printf("\nSJF Non-Preem\n");

    while (completed < count) {
        min_burst = 999999;
        shortest_job_index = -1;

        for (int i = 0; i < count; i++) {
            if (sjf_processes[i].arrival_time <= current_time && sjf_processes[i].finish_time == -1) {
                if (sjf_processes[i].burst_time < min_burst) {
                    min_burst = sjf_processes[i].burst_time;
                    shortest_job_index = i;
                }
            }
        }

        if (shortest_job_index == -1) {
            current_time++;
        } else {
            Process *p = &sjf_processes[shortest_job_index];
            p->start_time = current_time;
            current_time += p->burst_time;
            p->finish_time = current_time;
            execution_order[order_index++] = shortest_job_index;
            completed++;
            offset += snprintf(gantt_chart + offset, sizeof(gantt_chart) - offset, "| %s ", p->pid);
        }
    }
    offset += snprintf(gantt_chart + offset, sizeof(gantt_chart) - offset, "|");
    print_gantt_chart(gantt_chart);
    calculate_and_print_metrics(sjf_processes, count);
}

void run_rr(Process processes[], int count, int quantum) {
    Process rr_processes[MAX_PROCESSES];
    initialize_processes(rr_processes, processes, count);
    Queue ready_queue = { .front = 0, .rear = 0 };
    int current_time = 0;
    int completed_count = 0;
    int process_in_queue[MAX_PROCESSES] = {0};
    char gantt_chart[512] = "";
    int offset = 0;

    printf("\nRound Robin, (q=%d)\n", quantum);
    for (int i = 0; i < count; i++) {
        if (rr_processes[i].arrival_time == 0) {
            enqueue(&ready_queue, i);
            process_in_queue[i] = 1;
        }
    }
    while (completed_count < count) {
        if (ready_queue.front == ready_queue.rear) {
            int next_arrival_time = INF;
            for (int i = 0; i < count; i++) {
                if (rr_processes[i].finish_time == -1) {
                    if (rr_processes[i].arrival_time < next_arrival_time) {
                        next_arrival_time = rr_processes[i].arrival_time;
                    }
                }
            }
            if (next_arrival_time == INF) break;
            if (current_time < next_arrival_time) {
                current_time = next_arrival_time;
            }
            for (int i = 0; i < count; i++) {
                if (rr_processes[i].arrival_time <= current_time && 
                    !process_in_queue[i] && 
                    rr_processes[i].finish_time == -1) 
                {
                    enqueue(&ready_queue, i);
                    process_in_queue[i] = 1;
                }
            }
            if (ready_queue.front == ready_queue.rear) break;
            continue;
        }
        int index = dequeue(&ready_queue);
        Process *p = &rr_processes[index];
        process_in_queue[index] = 0;
        if (p->start_time == -1) {
            p->start_time = current_time;
        }
        int execution_time = (p->remaining_time > quantum) ? quantum : p->remaining_time;
        offset += snprintf(gantt_chart + offset, sizeof(gantt_chart) - offset, "| %s ", p->pid);
        for (int i = 0; i < execution_time; i++) {
            current_time++;
            p->remaining_time--;
            for (int j = 0; j < count; j++) {
                if (rr_processes[j].arrival_time == current_time && 
                    !process_in_queue[j] && 
                    rr_processes[j].finish_time == -1) 
                {
                    enqueue(&ready_queue, j);
                    process_in_queue[j] = 1;
                }
            }
        }
        if (p->remaining_time == 0) {
            p->finish_time = current_time;
            completed_count++;
        } else {
            enqueue(&ready_queue, index);
            process_in_queue[index] = 1;
        }
    }
    offset += snprintf(gantt_chart + offset, sizeof(gantt_chart) - offset, "|");
    print_gantt_chart(gantt_chart);
    calculate_and_print_metrics(rr_processes, count);
}

void run_scenario1(void) {
    Process scenario[] = {
        {"P1", 0, 8, 0, -1, -1, 0, 0},
        {"P2", 0, 4, 0, -1, -1, 0, 0},
        {"P3", 0, 1, 0, -1, -1, 0, 0},
        {"P4", 0, 3, 0, -1, -1, 0, 0}
    };
    int count = 4;

    printf("\nFirst Scenario:\n");
    run_fcfs(scenario, count);
    run_sjf(scenario, count);
    run_rr(scenario, count, 1);
    run_rr(scenario, count, 4);
}

void run_scenario2(void) {
    Process scenario[] = {
        {"P1", 0, 7, 0, -1, -1, 0, 0},
        {"P2", 2, 4, 0, -1, -1, 0, 0},
        {"P3", 4, 1, 0, -1, -1, 0, 0},
        {"P4", 5, 4, 0, -1, -1, 0, 0}
    };
    int count = 4;

    printf("\nSecond Scenario:\n");
    run_fcfs(scenario, count);
    run_sjf(scenario, count);
    run_rr(scenario, count, 1);
    run_rr(scenario, count, 4);
}

void run_scenario3(void) {
    Process scenario[] = {
        {"P1", 0, 20, 0, -1, -1, 0, 0},
        {"P2", 0, 3, 0, -1, -1, 0, 0},
        {"P3", 0, 3, 0, -1, -1, 0, 0}
    };
    int count = 3;

    printf("\nThird Scenario:\n");
    run_fcfs(scenario, count);
    run_sjf(scenario, count);
    run_rr(scenario, count, 1);
    run_rr(scenario, count, 4);
}

int main(int argc, char **argv) {

    run_scenario1();
    run_scenario2();
    run_scenario3();

    return 0;
}