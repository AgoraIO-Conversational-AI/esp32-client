
#ifndef AI_AGENT_H
#define AI_AGENT_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

int ai_agent_load_config(void);

/* start ai agent */
int ai_agent_start(void);

/* stop ai agent */
void ai_agent_stop(void);

#ifdef __cplusplus
}
#endif
#endif
