#include <iostream>

#include <CoFSM.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <mutex>
// Include headers needed for keyboard LED control is LINUX flag is defined.
#if defined(LINUX) && __has_include(<sys/ioctl.h>) && __has_include(<sys/kd.h>) && __has_include(<fcntl.h>) && __has_include(<unistd.h>)
#   include <sys/ioctl.h>
#   include <sys/kd.h>
#   include <fcntl.h>
#   include <unistd.h>
#   define USE_KEYBOARD_LEDS 1
#else
#   define USE_KEYBOARD_LEDS 0
#endif

extern std::mutex ledMutex;

using std::cout;

using CoFSM::FSM;
using CoFSM::Event;

// Mockup for Blue LED controller
class BlueLedControl
{
public:
     enum class Status { Off, On };
private:
    Status _ledStatus = Status::Off;
#if USE_KEYBOARD_LEDS
    static constexpr unsigned char _enableCode = 0x1;
    int _fdConsole = -1; // File descriptor for linux console
public:
    BlueLedControl()
    {
        _fdConsole = open("/dev/console", O_WRONLY);
        if (_fdConsole == -1) {
            std::cerr << "Error opening console file descriptor.\n"
                      << "Run the application with sudo or 'make clean' and make without linux argument.\n";
            std::terminate();
        }
        unsigned char led;
        ioctl(_fdConsole, KDGETLED, &led);
        ioctl(_fdConsole, KDSETLED, led & ~_enableCode);  // Turn off
    }
#else
public:
    BlueLedControl() = default;
#endif

    void set(Status value)
    {
        std::lock_guard<std::mutex> lock(ledMutex);
        _ledStatus = value;

#if USE_KEYBOARD_LEDS
        {
            unsigned char led;
            ioctl(_fdConsole, KDGETLED, &led);
            switch (value) {
            case Status::On:
                ioctl(_fdConsole, KDSETLED, led | _enableCode);
                break;
            case Status::Off:
                ioctl(_fdConsole, KDSETLED, led & ~_enableCode);
                break;
            }
        }
#endif
        std::cout << "Blue  LED = " << (_ledStatus == Status::Off ? "Off" : "On") << '\n';
    }
    Status get() const { return _ledStatus; }
};


// FSM state coroutine which turns the LED on
// and keeps it on for the time given in the activation event.
static CoFSM::State blueActiveState(FSM& fsm)
{
    BlueLedControl ledControl;
    CoFSM::Event event = co_await fsm.getEvent(); // Await for the first event
    while (true)
    {
        if (int* pOnTimeMs; event == "StartBlinkEvent")
        {  // Keep the LED on for the given time in milliseconds.
            ledControl.set(BlueLedControl::Status::On);
            // Note: "event >> pOnTimeMs" sets the pointer to point at the payload data
            //        and returns a reference to the payload.
            std::chrono::milliseconds msOnTime{std::max(0, event >> pOnTimeMs)};
            std::this_thread::sleep_for(msOnTime);
            ledControl.set(BlueLedControl::Status::Off);
            // Recycle the event from "StartBlink" to "BlinkReady".
            event.construct("BlinkReadyEvent");
        }
        else
        { // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());
        }
        event = co_await fsm.emitAndReceive(&event);  // Send the event and receive the next one.
    }
}

// FSM state coroutine which sends blink commands and takes care of handovers to and from this FSM.
static CoFSM::State blueIdleState(FSM& fsm)
{
    constexpr int iBlinkTimeMs = 252;   // Time of a single blink in milliseconds
    constexpr int iNumberOfBlinks = 2;  // Number of blinks to be done before handing over to the next FSM.
    std::stop_token stopToken;
    int iBlinksLeft = 0;
    Event event = co_await fsm.getEvent(); // Await for the first event
    while (true)
    {
        if (std::stop_token* pStop; event == "HandOverEvent")  // This FSM gets control from another FSM
        {
            event >> pStop;     // Stop token is in the payload of the handover event.
            stopToken = std::move(*pStop);
            iBlinksLeft = iNumberOfBlinks; // Do this many blinks before handing over to another FSM
            event.construct("StartBlinkEvent", iBlinkTimeMs); // iBlinkTimeMs piggybacks on "StartBlinkEvent"
        }
        else if (event == "BlinkReadyEvent")  // A blink is ready.
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(iBlinkTimeMs));
            if (stopToken.stop_requested())
                event.destroy();  // Send an empty event. It will suspend the FSM.
            else if (iBlinksLeft && (--iBlinksLeft) > 0)  { // Do more blinks?
                event.construct("StartBlinkEvent", iBlinkTimeMs);
            } else { // No more blinks, hand over to the next FSM by sending "HandOverEvent".
                event.construct("HandOverEvent", std::move(stopToken));
            }
        }
        else
        { // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());
        }

        event = co_await fsm.emitAndReceive(&event);  // Send the event and receive the next one.
    }
}

FSM* makeBlueFsm()
{
    static FSM blueFSM("BLUE-FSM");
    using CoFSM::transition;
    // Register and name the states.
    blueFSM << (blueIdleState(blueFSM)   = "BlueIdleState")
            << (blueActiveState(blueFSM) = "BlueActiveState");

    // Configure the transition table:
    //   When BlinkReady event is sent from Active state, go to Idle state.
    blueFSM << transition("BlueActiveState", "BlinkReadyEvent",  "BlueIdleState")
    //   When StartBlink event is sent from Idle state, go to Active state.
            << transition("BlueIdleState", "StartBlinkEvent",  "BlueActiveState")
    //   HandOver event (which contains stop token) is sent to self
            << transition("BlueIdleState", "HandOverEvent",  "BlueIdleState");

    return &blueFSM;
}
