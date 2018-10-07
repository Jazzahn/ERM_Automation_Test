#include <EthernetUdp2.h>
#include <EthernetClient.h>
#include <Dhcp.h>
#include <EthernetServer.h>
#include <Ethernet2.h>
#include <util.h>
#include <Dns.h>
#include <Twitter.h>




#define BUTTON_GL_PIN 41
#define BUTTON_GR_PIN 40
#define BUTTON_RL_PIN 37
#define BUTTON_RR_PIN 36
#define RELAY_PIN 30

bool useDHCP = false;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0x2C, 0xF7, 0xF1, 0x08, 0x18, 0x18
};

IPAddress ip(10, 1, 20, 230);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

// Track the room game state
bool phaseOneTriggered = false;  // has phase one been triggered
bool phaseTwoTriggered = false;  // has phase two been triggered





void setup() {
  //start serial connection (debugging)
  Serial.begin(9600);

  // NOTE: pin 1 & 2 are used for Serial
  // 4 & 10 - 13 are used for ethernet

  pinMode(BUTTON_GL_PIN, OUTPUT);   // Left Green Game btn
  pinMode(BUTTON_GR_PIN, OUTPUT);   // Right Green Game btn
  pinMode(BUTTON_RL_PIN, OUTPUT);   // Left Red Game btn
  pinMode(BUTTON_RR_PIN, OUTPUT);   // Right Red Game btn
  pinMode(RELAY_PIN, OUTPUT);   // Relay for Surge Protector

  // start the Ethernet connection and the server:
  Serial.println("Not using DHCP");
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP());
  
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());


  // Initial game state
  phaseOneTriggered = false;
  phaseTwoTriggered = false;
}



void loop() { 
  //read the pushbutton value into a variable
  int gameBtnGLVal = digitalRead(BUTTON_GL_PIN);
  int gameBtnGRVal = digitalRead(BUTTON_GR_PIN);
  int gameBtnRLVal = digitalRead(BUTTON_RL_PIN);
  int gameBtnRRVal = digitalRead(BUTTON_RR_PIN);

  int c = gameBtnGLVal + gameBtnGRVal + gameBtnRLVal + gameBtnRRVal;
  
  //print out the value of the pushbutton
  Serial.println(c);

  // Keep in mind the pullup means the pushbutton's
  // logic is inverted. It goes HIGH when it's open,
  // and LOW when it's pressed
  if (gameBtnGLVal == HIGH && gameBtnGRVal == HIGH) {
    triggerPhaseOne();
  }

  if (gameBtnRLVal == HIGH && gameBtnRRVal == HIGH && phaseOneTriggered == true) {
    triggerPhaseTwo();
  }

  // listen for incoming Ethernet connections:
  listenForEthernetClients();

  // Maintain DHCP lease
  if (useDHCP) {
    Ethernet.maintain();
  }


  // Use serial for debugging to bypass web server
  if (Serial.available()) {
    //read serial as a character
    char ser = Serial.read();

    switch (ser) {
      case 'r': // reset prop
        reset(); 
        break;

      case 'b': // button press
        triggerPhaseOne();
        break;  
    }
  }
}


/*
 * Game states
 */


// actions after button has been pressed
void triggerPhaseOne() {
  // While the code below is safe to run multiple times
  // it's best not to re-run it in case an expensive operation
  // is later added
  if (phaseOneTriggered) {
    return;
  }
  
  phaseOneTriggered = true;
}

void triggerPhaseTwo() {
  if (phaseTwoTriggered) {
    return;
  }

  phaseTwoTriggered = true;
  digitalWrite(RELAY_PIN, HIGH);
}

// Reset the prop
void reset() {
  phaseOneTriggered = false;
  phaseTwoTriggered = false;
  digitalWrite(RELAY_PIN, LOW);
}


/*
 * URL routing for prop commands and polling
 */

// This function dictates what text is returned when the prop is polled over the network
String statusString() {
 
  String pOne = phaseOneTriggered ? "phase one triggered" : "phase one not triggered";
  String pTwo = phaseTwoTriggered ? "phase two triggered" : "phase two not triggered";
  String returnString = pOne + " :: " + pTwo;

  return returnString;
}

// Actual request handler
void processRequest(EthernetClient& client, String requestStr) {
  Serial.println(requestStr);

  // Send back different response based on request string
  if (requestStr.startsWith("GET /status")) {
    Serial.println("polled for status!");
    writeClientResponse(client, statusString());
  } else if (requestStr.startsWith("GET /reset")) {
    Serial.println("Room reset");
    reset();
    writeClientResponse(client, "ok");
  } else if (requestStr.startsWith("GET /trigger")) {
    Serial.println("Network prop trigger");
    triggerPhaseOne();
    writeClientResponse(client, "ok");
  } else {
    writeClientResponseNotFound(client);
  }
}


/*
 * HTTP helper functions
 */

void listenForEthernetClients() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("Got a client");
    // Grab the first HTTP header (GET /status HTTP/1.1)
    String requestStr;
    boolean firstLine = true;
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          processRequest(client, requestStr);
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
          firstLine = false;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;

          if (firstLine) {
            requestStr.concat(c);
          }
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}


void writeClientResponse(EthernetClient& client, String bodyStr) {
  // send a standard http response header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Access-Control-Allow-Origin: *");  // ERM will not be able to connect without this header!
  client.println();
  client.print(bodyStr);
}


void writeClientResponseNotFound(EthernetClient& client) {
  // send a standard http response header
  client.println("HTTP/1.1 404 Not Found");
  client.println("Access-Control-Allow-Origin: *");  // ERM will not be able to connect without this header!
  client.println();
}
