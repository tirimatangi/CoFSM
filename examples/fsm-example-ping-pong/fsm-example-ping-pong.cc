#include <iostream>
#include <CoFSM.h>

using CoFSM::FSM;
using CoFSM::Event;
using std::cout;

// Coroutine which represents state Ping
CoFSM::State statePing(CoFSM::FSM& fsm)
{
    Event event = co_await fsm.getEvent(); // Await for the first event.
    while (true) {
        if (int* pCounter; event == "ToPingEvent")
        {
            event >> pCounter; // Get pointer to the data of the event
            if (*pCounter > 0) // Send ToPongEvent if the counter is still positive
                // Re-construct the event from Ping to Pong
                event.construct("ToPongEvent", *pCounter - 1);
            else // Send an empty event to suspend the FSM
                event.destroy();
        }
        else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        event = co_await fsm.emitAndReceive(&event);
    }
}

// Coroutine which represents state Pong
CoFSM::State statePong(CoFSM::FSM& fsm)
{
    Event event = co_await fsm.getEvent(); // Await for the first event.
    while (true) {
        if (int* pCounter; event == "ToPongEvent")
        {
            event >> pCounter; // Get pointer to the data of the event
            if (*pCounter > 0) // Send ToPongEvent if the counter is still positive
                // Re-construct the event from Pong to Ping
                event.construct("ToPingEvent", *pCounter - 1);
            else // Send an empty event to suspend the FSM
                event.destroy();
        }
        else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        event = co_await fsm.emitAndReceive(&event);
    }
}

// debug helper which tracks the transtions within an FSM.
struct Logger
{
    std::ostream& str;
    void operator()(const FSM& fsm, const std::string& fromState, const Event& onEvent, const std::string& toState)
    {
        str << " [" << fsm.name() <<"] '" << onEvent.name()
            << "' sent from '" << fromState << "' --> '" << toState << "'\n";
    }
};

/* Creates the states and sets the transition table like so:
   [ ping]  --- ToPongEvent ---> [ pong]
   [State] <--- ToPingEvent ---  [State]
*/
FSM& setup(FSM& fsm)
{
    using namespace CoFSM;

    // Make and name the states
    fsm << (statePing(fsm) = "pingState")
        << (statePong(fsm) = "pongState");

    // Set the transition table.
    fsm << transition("pingState", "ToPongEvent", "pongState")
        << transition("pongState", "ToPingEvent", "pingState");

    // List the states.
    cout << "'" << fsm.name() << "' has " << fsm.numberOfStates() << " states.\n";
    cout << "The states are:\n";
    for (unsigned i = 0; i < fsm.numberOfStates(); ++i)
        cout << "  (" << i << ") " << fsm.getStateAt(i).getName() << '\n';

    // List the transitions
    cout << "The transitions are:\n";
    auto allTransitions = fsm.getTransitions();
    for (const auto& tr : allTransitions) // tr is an array of 3 string_views (from, event, to)
        cout << "  {" << tr[0] << ',' << tr[1] << "} --> " << tr[2] << '\n';

    // Log the events to std:.cerr stream.
    fsm.logger = Logger{std::cerr};
    return fsm;
}

int main()
{
    FSM fsm{"PingPongFSM"};

    // Create the states and the transition table and start the state coroutines
    setup(fsm).start();

    // Make the first event which starts the show
    Event e;

    // Set the intial state as Ping and sent the first event to it.
    // Now the ping-pong-loop will run 2 times after which the FSM will suspend.
    cout << "\nRunning...\n";
    e.construct("ToPingEvent", 2); // Do the ping<->pong 2 times.
    fsm.setState("pingState").sendEvent(&e); // Send ToPingEvent to pingState.

    // Now we should be back at Ping state.
    cout << fsm.name() << " suspended at state " << fsm.currentState() << '\n';

    // Do it again but this time start from Pong state.
    cout << "\nRunning...\n";
    e.construct("ToPongEvent", 2);
    fsm.setState("pongState").sendEvent(&e); // Send ToPongEvent to pongState.

    // Now we should be back at Pong state.
    cout << fsm.name() << " suspended at state " << fsm.currentState() << '\n';

 return 0;
}

/*
Output:
'PingPongFSM' has 2 states.
The states are:
  (0) pingState
  (1) pongState
The transitions are:
  {pongState,ToPingEvent} --> pingState
  {pingState,ToPongEvent} --> pongState

Running...
 [PingPongFSM] 'ToPongEvent' sent from 'pingState' --> 'pongState'
 [PingPongFSM] 'ToPingEvent' sent from 'pongState' --> 'pingState'
PingPongFSM suspended at state pingState

Running...
 [PingPongFSM] 'ToPingEvent' sent from 'pongState' --> 'pingState'
 [PingPongFSM] 'ToPongEvent' sent from 'pingState' --> 'pongState'
PingPongFSM suspended at state pongState
*** Exited normally ***
*/