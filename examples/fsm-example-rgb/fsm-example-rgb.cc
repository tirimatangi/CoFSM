#include <iostream>
#include <CoFSM.h>
#include <thread>
#include <atomic>
#include <mutex>

using std::cout;

using CoFSM::FSM;
using CoFSM::Event;
std::mutex ledMutex;

// FSM factories defined elsewhere.
FSM* makeRedFsm();
FSM* makeGreenFsm();
FSM* makeBlueFsm();

// Helper for state transition tracing
struct Logger
{
    enum OnOff {Off, On};
    OnOff onOff = On; // Master switch. Off means print nothing.
    bool printThreadId = false;
    void operator()(const std::string& fsm, const std::string& fromState, const Event& onEvent, const std::string& toState)
    {
        if (onOff == Off)
            return;
        if (printThreadId)
            atomic_print(fsm, " : event '", onEvent.name(),
                        "' from '", fromState, "' to '", toState, "', thread id = 0x", std::hex, std::this_thread::get_id(), std::dec);
        else
            atomic_print(fsm, " : event '", onEvent.name(), "' from '", fromState, "' to '", toState, "'");
    }

    template <class... Args>
    void atomic_print(Args&&... args)
    {
        std::lock_guard<std::mutex> lock(ledMutex);
        (std::cout << ... << args) << '\n';
    }
};

using namespace std::literals::chrono_literals;

int main()
{
    // Make FSMs for red, green and blue devices.
    // The factories are living in separate source files which are compiled independently.
    FSM* redFSM = makeRedFsm();
    FSM* greenFSM = makeGreenFsm();
    FSM* blueFSM = makeBlueFsm();

    // Connect the FSMs by setting inter-FSM transitions so that
    // the FSMs run cyclically in order red-->green-->blue.
    using CoFSM::transition;
    // When the red FSM sends a handover event, the green FSM will take over.
    (*redFSM)   << transition("RedIdleState", "HandOverEvent",  "GreenIdleState", greenFSM);
    // When the green FSM sends a handover event, the blue FSM will take over.
    (*greenFSM) << transition("GreenIdleState", "HandOverEvent",  "BlueIdleState", blueFSM);
    // When the blue FSM sends a handover event, the red FSM will take over.
    (*blueFSM)  << transition("BlueIdleState", "HandOverEvent",  "RedIdleState", redFSM);

    // Activate the FSMs and set their respective intial states.
    redFSM->start().setState("RedIdleState");
    greenFSM->start().setState("GreenIdleState");
    blueFSM->start().setState("BlueIdleState");

    // Activate tracing.
    redFSM->logger = greenFSM->logger = blueFSM->logger = Logger{};

    // Start the action by sending a handover event to the given FSM.
    auto kickOff = [&](std::stop_token stopToken, FSM* fsm) {
        Event e;
        // The stop token of the thread piggy-backs to the fsm on the HandOver event.
        e.construct("HandOverEvent", stopToken);
        fsm->sendEvent(&e);
    };

    // Tell which FSMs are running
    auto printActive = [&](std::string str) {
        Logger().atomic_print(str,
                              " RED active = ", redFSM->isActive(),
                              ", GREEN active = ", greenFSM->isActive(),
                              ", BLUE active = ", blueFSM->isActive());
    };

    // Run the combined FSM sequentially 3 times, each time starting from a different state.
    // Note that stop_token requests the stop automatically when a jthread
    // is about to go out of scope.
    {
        cout << "---------------- Start the cycle with RED ----------------\n";
        std::jthread thread(kickOff, redFSM);
        std::this_thread::sleep_for(3000ms); // Sleep, then request stop.
        printActive("Activity before stop:"); // Only 1 of 3 should be running
    }
    {
        cout << "---------------- Start the cycle with GREEN ---------------- \n";
        std::jthread thread(kickOff, greenFSM);
        std::this_thread::sleep_for(3000ms); // Sleep, then request stop.
        printActive("Activity before stop:"); // Only 1 of 3 should be running
    }
    {
        cout << "---------------- Start the cycle with BLUE ----------------\n";
        std::jthread thread(kickOff, blueFSM);
        std::this_thread::sleep_for(3000ms); // Sleep, then request stop.
        printActive("Activity before stop:"); // Only 1 of 3 should be running
    }
    printActive("Activity after stop:"); // All 3 should be stopped.

    // Re-configure transitions so that each FSM "hands over" to itself instead of another FSM,
    // making the FSMs independent.
    (*redFSM)   << transition("RedIdleState", "HandOverEvent",  "RedIdleState");
    (*greenFSM) << transition("GreenIdleState", "HandOverEvent",  "GreenIdleState");
    (*blueFSM)  << transition("BlueIdleState", "HandOverEvent",  "BlueIdleState");

    // Show also the thread id in the trace.
    redFSM->logger = greenFSM->logger = blueFSM->logger = Logger{.printThreadId = true};
    // Now the FSMs are independent and can run in parallel.
    {
        cout << "---------------- Run RED, GREEN, BLUE in parallel ----------------\n";
        std::jthread redThread(kickOff, redFSM);
        std::jthread greenThread(kickOff, greenFSM);
        std::jthread blueThread(kickOff, blueFSM);
        std::this_thread::sleep_for(2s);
        printActive("All 3 are running in parallel:"); // All 3 should be active
    }
    printActive("All 3 have stopped:"); // All 3 should be inactive

    cout << "RED   fsm is suspended at state " << redFSM->currentState() <<  '\n';
    cout << "GREEN fsm is suspended at state " << greenFSM->currentState() <<  '\n';
    cout << "BLUE  fsm is suspended at state " << blueFSM->currentState() <<  '\n';
}