#ifndef __STATE_H__
#define __STATE_H__
/*
 *  A finite state machine
 */

typedef struct state* state_t;

extern state_t state_new(int default_state);

extern void transition(state_t);

extern void transition_to(state_t, int);

extern void transition_timer(void *);

extern void set_state(state_t);

extern int get_state(state_t);

extern void wait_for_transition(state_t);

extern void state_destroy(state_t);

#endif /*__STATE_H__*/
