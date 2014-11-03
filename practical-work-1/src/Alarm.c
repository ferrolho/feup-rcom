#include "Alarm.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "DataLink.h"

int alarmWentOff = 0;

void alarmHandler(int signal) {
	if (signal != SIGALRM)
		return;

	alarmWentOff = TRUE;
	ll->stats->timeouts++;
	printf("Connection time out!\n\nRetrying:\n");

	alarm(ll->timeout);
}

void setAlarm() {
	struct sigaction action;
	action.sa_handler = alarmHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	sigaction(SIGALRM, &action, NULL);

	alarmWentOff = FALSE;

	alarm(ll->timeout);
}

void stopAlarm() {
	struct sigaction action;
	action.sa_handler = NULL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	sigaction(SIGALRM, &action, NULL);

	alarm(0);
}
