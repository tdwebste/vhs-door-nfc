#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__


struct MainStateMachine {
    enum State_e {
        STATE_Idle,
        STATE_ValidatingRFID,
        STATE_IsVHSOpen,
        STATE_WaitingForPIN,
        STATE_ValidatingPIN,
        STATE_AccessGranted,

        STATE_COUNT
    };

    typedef void (*StateChangeCallback)(State_e oldState, State_e newState);

public:
    static const char* GetStateName(State_e state);

public:
    MainStateMachine();

    void init(StateChangeCallback callback);

    void    SetState(State_e newState);
    State_e GetState() const { return currentState; }

    void Update();

private:
    State_e             currentState;
    StateChangeCallback stateChangeCallback;

    int64_t stateTime;
};


#endif //__STATE_MACHINE_H__
