#include <SoftwareSerial.h>
#include <TimerOne.h>

SoftwareSerial wireless(4, 5); // RX, TX

long unsigned int shutdowntimer;
bool flag, blinkpowerstate, shutdown;
String options[6];

class Comms
// This class requires a software serial connection called "wireless" to be declared
{
public:
    String devicename;
    String incomingmessage;

    Comms(String deviceName)
    {
        devicename = deviceName;
    }

    // begins the serial connection to the wireless module
    void begin(int baudrate)
    {
        wireless.begin(baudrate);
    }

    // listens on the serial port, use inside of a loop
    void listen()
    {
        if (wireless.available())
        {
            record();
            respond();
        }
    }

    // broadcasts a message (no ack)
    void broadcast(String message)
    {
        // sends the message
        wireless.print(char(10));
        wireless.print(devicename);
        wireless.print(" ");
        wireless.print(message);
        wireless.print(char(13));
    }

    // sends a message (expects ack)
    void send(String target, String command)
    {
        // check if target is itself
        if (target == devicename)
        {
            // bounce it back to inbox and not send it
            setmessage(devicename + " " + target + " " + command);
            return;
        }

        // else, send the message
        for (int i = 0; i < 3; i++)
        {
            // sends the message
            wireless.print(char(10));
            wireless.print(devicename);
            wireless.print(" ");
            wireless.print(target);
            wireless.print(" ");
            wireless.print(command);
            wireless.print(char(13));
            // after that, listen for ack
            if (waitforack())
            {
                return;
            }
        }
        setmessage("! ack timeout");
        return;
    }

    // announces itself and ask other devices to register itself
    void announce()
    {
        // <source> announce // asks nearby devices to respond with their names
        wireless.print(char(10) + devicename + " announce" + char(13));
    }

    void atinit(bool atstate)
    {
        digitalWrite(6, atstate);
    }

    void setmessage(String txt)
    {
        message += char(10) + txt + char(13);
    }

    void nextmessage()
    {
        String txt = "";
        bool issecondmessage = false;
        for (int i = 0; i < message.length(); i++)
        {
            if (issecondmessage)
            {
                txt += message.charAt(i);
            }

            if (message.charAt(i) == 13)
            {
                issecondmessage = true;
            }
        }
        message = txt;
    }

    bool newmsgavailable()
    {
        if (newmessage)
        {
            newmessage = false;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool available()
    {
        return message.length() != 0;
    }

    // returns the first segment and clears the available state
    String read()
    {
        String txt = "";
        for (int i = 0; i < message.length(); i++)
        {
            switch (int(message.charAt(i)))
            {
            case 10:
                break;
            case 13:
                return txt;
            default:
                txt += message.charAt(i);
                break;
            }
        }
    }

    // returns the argument of requestedItem
    // e.g. txt = "hello world", requestedItem = 1, returning string would be "world"
    String pharse(String txt, int requestedItem)
    {
        String response = "";
        int currentItem = 0;
        bool inquotes = false;
        // go through the length of the text, one at a time
        for (int i = 0; i < txt.length(); i++)
        {
            if (txt.charAt(i) == ' ' && !inquotes)
            // checks if it is a space and not in a quote
            {
                // if so, increment item count
                currentItem++;

                // checks if the last item is the one that's requested
                if (currentItem > requestedItem)
                {
                    // if so, return the recorded string
                    return response;
                }
                // if not, then clear buffer and start recording the next segment
                response = "";
            }
            else if (txt.charAt(i) == '"')
            // check if it is a quotemark
            {
                // if so, toggle inquotes
                inquotes = !inquotes;
            }
            else
            {
                // if not, just record it into buffer for future use
                response += String(txt.charAt(i));
            }
        }

        // fallback check for if the requested item is the last item.
        // in this case, the above mechanism would not trigger
        // because there isn't a space at the end of transmission
        if (currentItem == requestedItem)
        {
            return response;
        }
        return "-1";
    }

private:
    char incomingchar;
    String message;
    bool newmessage;

    bool waitforack()
    {
        long unsigned int acktimeout = millis();
        while (millis() - acktimeout < 1000)
        {
            if (wireless.available())
            {
                if (wireless.read() == char(6))
                {
                    return true;
                }
            }
        }
        return false;
    }

    // records incoming messages
    void record()
    {
        long unsigned int timer = millis();
        while (true)
        {
            // wait until a byte comes in
            while (!wireless.available())
            {
                // check if it's timed out
                if (millis() - timer > 1000)
                {
                    // timed out
                    incomingchar = 0;
                    incomingmessage = "";
                    setmessage("! incoming timeout");
                    return;
                }
            }

            // reset timeout timer
            timer = millis();

            // reads the incoming byte
            incomingchar = wireless.read();

            // detect end of transmission
            // filters control bytes, 10 starts message, 13 ends message
            switch (incomingchar)
            {
            case 6:
                // ack byte
                break;
            case 10:
                // transmission starts, clear buffer
                incomingmessage = "";
                break;
            case 13:
                // transmission ends
                setmessage(incomingmessage);
                newmessage = true;
                return;
            default:
                incomingmessage += incomingchar;
                break;
            }
        }
    }

    void respond()
    {
        // check if target of the message is this machine
        if (pharse(incomingmessage, 1) == devicename)
        {
            // ack syntax: 6
            // do not use send(), otherwise there will be a broadcast catastrophe of acks in the air,
            // and the NSA would come knocking at your door "knack" "knack"
            wireless.print(char(6));
        }

        // check if message is a brodacasted query for available devices
        // <source> announce // asks nearby devices to respond with their names
        if (pharse(incomingmessage, 1) == "announce")
        {
            // wait until other devices finish transmitting
            while (wireless.available())
            {
                record();
                respond();
            }
            // <source> <target> online // tells target (everyone, actually) that device is online
            send(pharse(incomingmessage, 0), "online");
        }
    }
};

Comms comms("motion");

void setup()
{
    comms.begin(9600);
    pinMode(6, INPUT_PULLUP); // sensor pin
    pinMode(7, OUTPUT);       // blink power pin
    Timer1.initialize(5000000);
    Timer1.attachInterrupt(blinkpower);
    Serial.begin(9600);
    comms.broadcast("init");
}

void loop()
{
    // listen to commands
    comms.listen();

    if (comms.available())
    {
        commandtree(comms.read());
        comms.nextmessage();
    }

    // detect sensor readings
    if (digitalRead(6) && !flag)
    {
        flag = true;
        comms.broadcast("triggered");
        digitalWrite(13, HIGH);
    }
    if (!digitalRead(6) && flag)
    {
        flag = false;
        comms.broadcast("untriggered");
        digitalWrite(13, LOW);
    }
}

void remotemenutree(String target)
// tree location not zero-indexed, 0 reserved for "return"
{
    options[0] = F("Firmware Info");
    options[1] = F("Shutdown");
    switch (remotemenu(2, comms.devicename, options, target))
    {
    case 1:
        comms.broadcast("v0.4");
        comms.broadcast(__DATE__);
        break;
    case 2:
        int shutdowntime = remoteselnum(F("Schedule Shutdown"), 0, 60, 0, target);
        if (shutdowntime != -1)
        {
            shutdowntimer = 60000 * shutdowntime + millis();
            comms.broadcast("shutdown scheduled");
        }
        break;
    }
}

int remoteselnum(String title, int mini, int maxi, int defaultint, String target)
{
    // <source> <target> selnum "<title>" <mini> <maxi> <defaultint>
    comms.send(target, "selnum \"" + title + "\" " + String(mini) + " " + String(maxi) + " " + String(defaultint));

    // listen for response
    long unsigned int timeout = millis() + 30000;
    while (timeout > millis())
    {
        comms.listen();
        if (comms.newmsgavailable())
        {
            // checks if source of message is target and target is devicename
            if (comms.pharse(comms.incomingmessage, 0) == target && comms.pharse(comms.incomingmessage, 1) == comms.devicename)
            {
                // <source> <target> respond <value>
                if (comms.pharse(comms.incomingmessage, 2) == "respond")
                {
                    return comms.pharse(comms.incomingmessage, 3).toInt();
                }
            }
        }
    }
    return -1;
}

int remotemenu(int optcount, String title, String opts[], String target)
{
    // trigger the menu
    // send the options
    for (int i = 0; i < optcount; i++)
    {
        // <source> <target> set <index> "<content>"
        comms.send(target, "set " + String(i) + " \"" + opts[i] + "\"");
    }
    // trigger with <source> <target> menu <optcount> "<title>"
    comms.send(target, "menu " + String(optcount) + " \"" + title + "\"");

    // listen for response
    long unsigned int timeout = millis() + 30000;
    while (timeout > millis())
    {
        comms.listen();
        if (comms.newmsgavailable())
        {
            // checks if source of message is target and of target is devicename
            if (comms.pharse(comms.incomingmessage, 0) == target && comms.pharse(comms.incomingmessage, 1) == comms.devicename)
            {
                // <source> <target> respond <value>
                if (comms.pharse(comms.incomingmessage, 2) == "respond")
                {
                    return comms.pharse(comms.incomingmessage, 3).toInt();
                }
            }
        }
    }
    return -1;
}

void commandtree(String command)
{
    // checks target of command
    if (comms.pharse(command, 1) == comms.devicename)
    // command directed at this device
    {
        // checks command arguments
        if (comms.pharse(command, 2) == "ping")
        // <source> <target> ping // respond with "pong"
        {
            comms.send(comms.pharse(command, 0), "pong");
            return;
        }
        else if (comms.pharse(command, 2) == "pong")
        // <source> <target> pong // response of "ping", do nothing
        {
            return;
        }
        else if (comms.pharse(command, 2) == "negative")
        // <source> <target> negative // do nothing
        {
            if (comms.pharse(command, 3) == "invalid")
            // <source> <target> negative invalid // do nothing
            {
                return;
            }
        }
        else if (comms.pharse(command, 2) == "ack")
        // <source> <target> ack // do nothing
        {
            return;
        }
        else if (comms.pharse(command, 2) == "respond")
        // <source> <target> respond <value> // do nothing
        {
            return;
        }
        else if (comms.pharse(command, 2) == "query")
        // <source> <target> query // trigger remotemenutree
        {
            remotemenutree(comms.pharse(command, 0));
            return;
        }
        else
        // no command matches // respond with "negative invalid"
        {
            comms.send(comms.pharse(command, 0), "negative invalid");
            return;
        }
    }
}

void blinkpower()
{
    // shutdown check background process, checks if it's time to shutdown
    if (shutdowntimer != 0)
    {
        if (millis() > shutdowntimer)
        {
            shutdown = true;
            comms.broadcast("shutdown initiated");
        }
    }

    // change the state of the power pin to keep the chip awake
    if (shutdown)
    {
        digitalWrite(7, LOW);
    }
    else
    {
        blinkpowerstate = !blinkpowerstate;
        digitalWrite(7, blinkpowerstate);
    }
}
