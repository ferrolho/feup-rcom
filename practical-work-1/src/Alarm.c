#include "Alarm.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "DataLink.h"

int alarmWentOff = 0;

void alarmHandler(int signal) {
	if (signal != SIGALRM)
		return;

	alarmWentOff = 1;
	printf("Connection time out!\n\nRetrying:\n");

	alarm(ll->timeout);
}

void setAlarm() {
	signal(SIGALRM, alarmHandler);

	alarmWentOff = 0;

	alarm(ll->timeout);
}

void stopAlarm() {
	alarm(0);
}
