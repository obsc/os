#include "state.h"
#include "machineprimitives.h"
#include "synch.h"

struct state {
    int cur_state;

    semaphore_t transition; // Semaphore for transition blocking
    int transitioned; // Represents if transitioned yet
};

state_t state_new(int default_state) {
    state_t s = (state_t) malloc (sizeof(struct state));
    if ( !s ) return NULL;

    s->transition = semaphore_create();
    s->transitioned = 0;

    if (!s->transition) {
        semaphore_destroy(s->transition);
        free(s);
        return NULL;
    }

    s->cur_state = default_state;

    semaphore_initialize(s->transition, 0);
    return s;
}

void transition(state_t state) {
    if (swap(&state->transitioned, 1) == 0) {
        semaphore_V(state->transition);
    }
}

void transition_to(state_t state, int new_state) {
    state->cur_state = new_state;
    transition(state);
}

void transition_timer(void * state) {
    state_t s = (state_t) state;
    transition(s);
}

void set_state(state_t state, int new_state) {
    state->cur_state = new_state;
}

int get_state(state_t state) {
    return state->cur_state;
}

void wait_for_transition(state_t state) {
    semaphore_P(state->transition);
    state->transitioned = 0;
}

void state_destroy(state_t state) {
    if ( !state ) return;

    semaphore_destroy(state->transition);
    free(state);
}
