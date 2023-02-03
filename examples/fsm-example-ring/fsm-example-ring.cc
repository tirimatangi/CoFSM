#include <iostream>
#include <chrono>
#include <algorithm>

#include <CoFSM.h>

// FSM state coroutine which cycles through the ring of states several times
// after having received StartEvent and stores the average number of
// state transitions per second in *transitionsPerSecond.
CoFSM::State readyState(CoFSM::FSM& fsm, double& runningTimeSecs)
{
    int roundsLeft = 0;
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;
    enum class Direction {Clockwise, CounterClockwise};
    Direction direction = Direction::Clockwise;

    CoFSM::Event event = co_await fsm.getEvent(); // Await for the first event
    while (true) {
#if PRINT_EVENTS // An example of printing out the incoming event and some data of the current state.
        std::cerr << "readyState: event = " << event.name() << ", roundsLeft = " << roundsLeft << '\n';
#endif
        if (int* pNumberOfRounds; event == "StartEvent") { // Start the measurement
            event >> pNumberOfRounds;  // Get pointer to the number of rounds to run.
            roundsLeft = std::max(*pNumberOfRounds, 1);
            startTime = std::chrono::high_resolution_clock::now();
        } else if (event == "ClockwiseEvent") { // One round of the ring of states done
            // The next round will circulate at the opposite direction
            direction = Direction::CounterClockwise;
        } else if (event == "CounterClockwiseEvent") { // One round of the ring of states done
            // The next round will circulate at the opposite direction
            direction = Direction::Clockwise;
        } else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        if (roundsLeft > 0) { // Start another round of states in the given direction
            --roundsLeft;
            if (direction == Direction::Clockwise)
                event.construct("ClockwiseEvent");
            else
                event.construct("CounterClockwiseEvent");
        } else { // The required number of rounds done.
            endTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = endTime - startTime;
            runningTimeSecs += diff.count();
            event.destroy(); // Suspend the FSM by sending an empty event
        }

        event = co_await fsm.emitAndReceive(&event);  // Send the event and receive the next one.
    }
}


// A state on the ring of states.
// Passes either clockwise or counter-clockwise token event to the next state on the ring.
CoFSM::State ringState(CoFSM::FSM& fsm, unsigned& numEventsProcessed)
{
    CoFSM::Event event = co_await fsm.getEvent(); // Await for the first event
    while (true) {
        if (event == "ClockwiseEvent" || event == "CounterClockwiseEvent") {
            // An expected event was received.
            ++numEventsProcessed;
        }  else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        event = co_await fsm.emitAndReceive(&event);  // Re-send the same event and receive the next one.
    }

}

int main()
{
    using namespace CoFSM;

    FSM ring{"Ring FSM"};

    double runningTimeSecs = 0;             // Total running time
    constexpr int statesInRing = 1023;      // Number of states in the ring
    constexpr int numRoundsToRepeat = 10000;   // Number of times the ring will be cycled through.
    unsigned numEventsProcessed = 0;        // Number of times a state in the ring has received an event

    // Register the states in the ring of states.
    // They don't need individual names as they will be referred by the number 0...statesInRing-1
    for (int i = 0; i < statesInRing; ++i)
        ring << ringState(ring, numEventsProcessed);

    // Configure transitions clockwise from state i to state i+1
    // and counter clockwise from state i+1 to state i.
    for (int i = 0; i < statesInRing-1; ++i) {
        ring << transition(ring.getStateAt(i), "ClockwiseEvent", ring.getStateAt(i+1));
        ring << transition(ring.getStateAt(i+1), "CounterClockwiseEvent", ring.getStateAt(i));
    }

    // Register and name the ready state where the ring of states begin and end.
    ring << (readyState(ring, runningTimeSecs) = "ready");

    // Configure transitions from ready state to/from the first and last states of the ring
    // in both directions
    ring << transition("ready", "ClockwiseEvent", ring.getStateAt(0))
         << transition(ring.getStateAt(statesInRing-1), "ClockwiseEvent", "ready")
         << transition("ready", "CounterClockwiseEvent", ring.getStateAt(statesInRing-1))
         << transition(ring.getStateAt(0), "CounterClockwiseEvent", "ready");

    // Launch the state coroutines and set the initial state.
    ring.start().setState("ready");

    // Make the first event which will start the show.
    Event e;
    e.construct("StartEvent", numRoundsToRepeat); // Cycle around the ring numRoundsToRepeat times.
    ring.sendEvent(&e);

    std::cout << ring.name() << "' is suspended at state '" << ring.currentState() << "'\n";
    std::cout << "Based on " << numRoundsToRepeat << " rounds around the ring of "
              << statesInRing << " states in " << runningTimeSecs <<" secs, meaning "
              << (numEventsProcessed + numRoundsToRepeat) << " events sent,\n"
              << "the speed of FSM's execution is "
              << (numEventsProcessed + numRoundsToRepeat) / runningTimeSecs
              << " state transitions per second\n";
    return 0;
}
