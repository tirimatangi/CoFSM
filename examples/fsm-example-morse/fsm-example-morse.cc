#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <string_view>
#include <vector>
#include <cctype>

#include <CoFSM.h>

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

using std::cout;

using CoFSM::FSM;
using CoFSM::Event;

class SoundControl
{
public:
     enum class Status { Off, On };
private:
    Status _soundStatus = Status::Off;
#if USE_KEYBOARD_LEDS
    static constexpr unsigned char _enableCode = 0x7;
    int _fdConsole = -1; // File descriptor for linux console
public:
    SoundControl()
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
    SoundControl() = default;
#endif

    void set(Status value)
    {
        _soundStatus = value;
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
        // std::cout << "Sound = " << (_soundStatus == Status::Off ? "Off" : "On") << '\n';
    }
};

// FSM state coroutine which sets the sound on
// and keeps it on for the time given in BeebEvent.
CoFSM::State soundOnState(CoFSM::FSM& fsm, SoundControl* soundControl)
{
    CoFSM::Event event = co_await fsm.getEvent(); // Await for the first event
    while (true) {
        if (unsigned* pBeepTimeMs; event == "DoBeepEvent") { // Beep for the given number of milliseconds
            event >> pBeepTimeMs;  // Get pointer to the on-time in milliseconds.
            soundControl->set(SoundControl::Status::On);
            std::this_thread::sleep_for(std::chrono::milliseconds{*pBeepTimeMs});
            soundControl->set(SoundControl::Status::Off);

            // Recycle the BeebEvent into BeebDoneEvent.
            event.construct("BeebDoneEvent");
        } else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        event = co_await fsm.emitAndReceive(&event);  // Send the event and receive the next one.
    }
}

// FSM state coroutine which controls the timings of dots and dashes.
// It receives a string of dots and dashes and transmits them one by one.
CoFSM::State transmissionInProgressState(CoFSM::FSM& fsm, int speedWordsPerMinute)
{
    const unsigned dotTimeInMs = 1200u / std::clamp(1, 1200, speedWordsPerMinute);
    const unsigned dashTimeInMs = 3 * dotTimeInMs;
    std::string_view symbol; // String of signals (i.e. dots and dashes)
    unsigned signalsTransmitted = 0; // Number of symbols transmitted so far.

    CoFSM::Event event = co_await fsm.getEvent(); // Await for the first event
    while (true) {
        if (std::string_view* pSymbol; event == "TransmitSymbolEvent") {
            // Start transmission of a new symbol which consists of signals (i.e. dots and dashes.)
            // Get a pointer to a string_view of signals with operator>> and return a reference to the string_view.
            symbol = (event >> pSymbol);
            signalsTransmitted = 0;
        }
        else if (event == "BeebDoneEvent") {
            // A signal has been transmitted. Insert a gap between dots and dashes.
            std::this_thread::sleep_for(std::chrono::milliseconds{dotTimeInMs});
        }  // BeepDoneEvent
        else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        // Transmit the next signal if any left in the symbol
        if (signalsTransmitted < symbol.size()) {
            char signal = symbol[signalsTransmitted++];
            std::cout << signalsTransmitted << " = " << signal << '\n';
            if (signal =='.')  // Transmit dot
                event.construct("DoBeepEvent", dotTimeInMs);
            else if (signal == '-')  // Transmit dash
                event.construct("DoBeepEvent", dashTimeInMs);
            else if (signal ==  ' ') {
                // Gap between words is 7 dots.
                // The gap between words, if present, is assumed to be the last signal of the symbol.
                signalsTransmitted = symbol.size();
                std::this_thread::sleep_for(std::chrono::milliseconds{7 * dotTimeInMs});
                event.construct("TransmissionReadyEvent");
            }
        } else { // The entire symbol has been transmitted. Complete symbol gap of 1+2 dot times.
            std::this_thread::sleep_for(std::chrono::milliseconds{2 * dotTimeInMs});
            event.construct("TransmissionReadyEvent");
        }

        event = co_await fsm.emitAndReceive(&event);  // Send the event and receive the next one.
    }
}

// FSM state coroutine which maps character strings into Morse symbols.
CoFSM::State transmitReadyState(CoFSM::FSM& fsm)
{
    std::unordered_map<char, std::string_view> map; // Map characters to morse codes
    map[' ']=" "; map['A']=".-"; map['B']="-..."; map['C']="-.-."; map['D']="-..";  map['E']=".";
    map['F']="..-."; map['G']="--."; map['H']="...."; map['I']=".."; map['J']=".---";
    map['K']="-.-"; map['L']=".-.."; map['M']="--"; map['N']="-."; map['O']="---";
    map['P']=".--."; map['Q']="--.-"; map['R']=".-."; map['S']="..."; map['T']="-";
    map['U']="..-"; map['V']="...-"; map['W']=".--";  map['X']="-..-"; map['Y']="-.--";  map['Z']="--..";
    map['1']=".----"; map['2']="..---"; map['3']="...--"; map['4']="....-"; map['5']=".....";
    map['6']="-...."; map['7']="--..."; map['8']="---.."; map['9']="----."; map['0']="-----";

    CoFSM::Event event = co_await fsm.getEvent(); // Await for the first event
    std::string message;
    unsigned symbolsSent = 0;
    while (true) {
        if (std::string* pString; event == "TransmitMessageEvent") {
            // Take the string from the event data and store.
            event >> pString;
            message = std::move(*pString);
            // String is not trivially destructible so destroy explicitly from the event data.
            event.destroy(pString);
            symbolsSent = 0;
        }
        else if (event == "TransmissionReadyEvent") {
            // A symbol has been transmitted. Do the next one, if any.
            if (symbolsSent == message.size()) // All symbols are sent.
                event.destroy(); // Suspend the FSM by sending an empty event.
        }
        else  // The event was not recognized.
            throw std::runtime_error("Unrecognized event '" + event.nameAsString() + "' received in state " + fsm.currentState());

        if (symbolsSent < message.size()) {
            char charOut = std::toupper(message[symbolsSent]);
            charOut = map.contains(charOut) ? charOut : ' ';
            std::cout << "--> '" << charOut << "'\n";
            event.construct("TransmitSymbolEvent", map.at(charOut));
            ++symbolsSent;
        }

        event = co_await fsm.emitAndReceive(&event);  // Send the event and receive the next one.
    }
}

// Helper for state transition tracing
struct Logger
{
    std::ostream& str;
    uint64_t count = 0;
    void operator()(const std::string& fsm, const std::string& fromState, const Event& onEvent, const std::string& toState)
    {
        str << (++count) << " # FSM '" << fsm <<"' : event '" << onEvent.name()
            << "' from '" << fromState << "' --> '" << toState << "'\n";
    }
};

int main()
{
    using namespace CoFSM;
    SoundControl soundController;
    int wordsPerMinute = 12; // Approximate transmission speed in words per minute.

    FSM morse{"Morse FSM"};

    // Register the state coroutines and give them names.
    morse << (transmitReadyState(morse) = "transmitReady")
          << (transmissionInProgressState(morse, wordsPerMinute) = "transmissionInProgress")
          << (soundOnState(morse, &soundController) = "soundOn");

    // Configure the transition table in format ("From State", "Event", "To State")
    // Example: "TransmitSymbolEvent" sent from "transmitReady" state goes to "transmissionInProgress" state
    morse << transition("transmitReady", "TransmitSymbolEvent", "transmissionInProgress")
          << transition("transmissionInProgress", "TransmissionReadyEvent", "transmitReady")
          << transition("transmissionInProgress", "DoBeepEvent", "soundOn")
          << transition("soundOn", "BeebDoneEvent", "transmissionInProgress");

    // Change to "#if 1" to use live tracing.
    // You can filter the log messages out by running "sudo ./fsm-example-morse 2> /dev/null"
    // The sudo is needed only if LEDs are used (i.e. built with "make linux")
#if 0
    morse.logger = Logger{std::cerr};
#endif

    // Launch the state coroutines and set the initial state.
    morse.start().setState("transmitReady");

    // Make the first event which will start the show.
    Event e;

    // Send these sentences
    std::vector<std::string> text {"Hello World ", "SOS SOS ", "Wikipedia the free encyclopedia"};
    for (const auto& str : text) {
        std::cout << "Message = '" << str << "'\n";
        e.construct("TransmitMessageEvent", str); // Repeat the on-off cycle 4 times
        morse.sendEvent(&e);
    }
    cout << "\n'" << morse.name() << "' is suspended at state '" << morse.currentState() << "'\n";

    return 0;
}
