
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_types.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include <esp_timer.h>

#include "utils.h"
#include "main_state_machine.h"


static const char* StateNames[MainStateMachine::STATE_COUNT] = {
    "Idle",
    "ValidatingRFID",
    "WaitingForPIN",
    "ValidatingPIN",
    "AccessGranted"
};

const char* MainStateMachine::GetStateName(State_e state) {
    return StateNames[state];
}

//
MainStateMachine::MainStateMachine()
    : currentState(STATE_Idle)
    , stateChangeCallback(NULL)
    , stateTime(0) {
}

void MainStateMachine::init(StateChangeCallback callback) {
    currentState        = STATE_Idle;
    stateChangeCallback = callback;
    stateTime           = esp_timer_get_time();
}

void MainStateMachine::SetState(State_e newState) {
    State_e oldState = currentState;
    currentState     = newState;

    stateTime = esp_timer_get_time();

    if (stateChangeCallback != NULL) {
        stateChangeCallback(oldState, newState);
    }
}

void MainStateMachine::Update() {
    if (currentState != STATE_Idle) {
        int64_t currentTime    = esp_timer_get_time();
        int64_t timeInState_uS = currentTime - stateTime;
        if (timeInState_uS > SECONDS_IN_US(15)) {
            SetState(STATE_Idle);
            return;
        }
    }

    if (currentState == STATE_AccessGranted) {
        // TODO: If the door sensor says the door is open, re-lock immediately so when the door closes it latches locked
        bool doorIsOpen = false;
        if (doorIsOpen) {
            SetState(STATE_Idle);
        }
    }
}
