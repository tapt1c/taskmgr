#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cad_policy.h"
static TM95_CAD_STATE state;
static void setup(unsigned int tick)
{
    memset(&state,0,sizeof(state));state.armed=1;state.heartbeat=tick;
    tm95_cad_key(&state,0x1d,tick);tm95_cad_key(&state,0x38,tick);
}
int main(void)
{
    setup(100);
    assert(!tm95_cad_key(&state,0x1e,100));
    assert(!tm95_cad_key(&state,0xe0,100));
    assert(tm95_cad_key(&state,0x53,100) && state.events==1);
    assert(tm95_cad_key(&state,0x53,200) && state.events==1);
    tm95_cad_key(&state,0xe0,200);
    assert(tm95_cad_key(&state,0xd3,200));
    assert(!tm95_cad_key(&state,0x53,300) && state.events==1);
    tm95_cad_key(&state,0xd3,300);
    state.heartbeat=2300;
    assert(tm95_cad_key(&state,0x53,2300) && state.events==2);
    setup(100);
    assert(!tm95_cad_key(&state,0x53,3101));
    setup(100);state.armed=0;
    assert(!tm95_cad_key(&state,0x53,100));
    setup(100);tm95_cad_key(&state,0xb8,100);
    assert(!tm95_cad_key(&state,0x53,100));
    setup(100);tm95_cad_key(&state,0x9d,100);
    assert(!tm95_cad_key(&state,0x53,100));
    setup(100);tm95_cad_key(&state,0xe1,100);
    tm95_cad_key(&state,0x1d,100);tm95_cad_key(&state,0x45,100);
    tm95_cad_key(&state,0xe1,100);tm95_cad_key(&state,0x9d,100);tm95_cad_key(&state,0xc5,100);
    assert(state.ctrl && state.alt && tm95_cad_key(&state,0x53,100));
    setup(0xfffffff0U);
    assert(tm95_cad_key(&state,0x53,16));
    memset(&state,0,sizeof(state));state.armed=1;state.heartbeat=100;
    tm95_cad_key(&state,0xe0,100);tm95_cad_key(&state,0x1d,100);
    tm95_cad_key(&state,0xe0,100);tm95_cad_key(&state,0x38,100);
    assert(state.ctrl==2 && state.alt==2 && tm95_cad_key(&state,0x53,100));
    puts("PASS: CAD policy, heartbeat fallback, second press, repeat, modifiers, Pause and tick wrap");
    return 0;
}
