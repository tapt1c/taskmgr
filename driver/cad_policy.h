#ifndef TM95_CAD_POLICY_H
#define TM95_CAD_POLICY_H
/* IRQ-safe policy: no allocation, user pointers, callbacks, or blocking. */
typedef struct {
    unsigned int heartbeat,last_intercept,events;
    unsigned char ctrl,alt,extended,delete_down,swallowed,armed,intercepted,pause_bytes;
} TM95_CAD_STATE;
static int tm95_cad_key(volatile TM95_CAD_STATE *s,unsigned int scan,unsigned int now)
{
    unsigned int key=scan&127,side;
    int release=(scan&128)!=0;
    if(s->pause_bytes) { --s->pause_bytes;return 0; }
    if(scan==0xe1) { s->pause_bytes=5;return 0; }
    if(scan==0xe0) { s->extended=1;return 0; }
    side=s->extended?2:1;s->extended=0;
    if(key==0x1d) { if(release) s->ctrl&=(unsigned char)~side;else s->ctrl|=(unsigned char)side;return 0; }
    if(key==0x38) { if(release) s->alt&=(unsigned char)~side;else s->alt|=(unsigned char)side;return 0; }
    if(key!=0x53) return 0;
    if(release) {
        int swallowed=s->swallowed;
        s->delete_down=s->swallowed=0;return swallowed;
    }
    if(s->delete_down) return s->swallowed;
    s->delete_down=1;
    if(!s->ctrl || !s->alt || !s->armed || (unsigned int)(now-s->heartbeat)>3000) return 0;
    /* A second deliberate press within two seconds reaches the native handler. */
    if(s->intercepted && (unsigned int)(now-s->last_intercept)<2000) return 0;
    s->last_intercept=now;s->intercepted=1;s->swallowed=1;++s->events;
    return 1;
}
#endif
