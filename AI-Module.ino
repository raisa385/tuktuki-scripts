#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "M31";       // Wi-Fi name
const char* password = "marshm3110w";     // Wi-Fi password

WebServer server(80);

// Variables to store robot instructions
String currentMode = "";
String currentAction = "";
int targetTable = 0;
String orderedItem = "";

// HTML page served by ESP32
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<body>
  <h2>Food Serving Robot Chat (JSON Response)</h2>
  <input type="text" id="userText" placeholder="Give command" size="50">
  <button onclick="window.sendText()">Send</button>
  <button onclick="window.startMic()">🎤 Mic (20s)</button>
  <pre id="output"></pre>

  <script src="https://js.puter.com/v2/"></script>
  <script>
    const menuItems = ["chocolate","biscuit","lollipop"];

    window.sendText = function() {
      const userCommand = document.getElementById("userText").value;
      if(!userCommand) return;

      document.getElementById("output").innerText = "Processing...";

      const systemPrompt = `
You are a restaurant robot waiter.
Always respond in JSON using ONLY the following structures:

1. General:
{ "mode": "general", "response": "<text>", "action": "none" }

2. Table selection:
{ "mode": "table_selection", "response": "Table <number> assigned", "action": "go_to_table", "target": <integer> }

3. Order selection:
{ "mode": "order_selection", "response": "Order #<number> confirmed: <item_name>", "action": "return_to_base", "item": "<item_name>" }

Available menu items: ${menuItems.join(", ")}.

Do not return anything outside this JSON format. Always output valid JSON only.`;

      puter.ai.chat(System: ${systemPrompt}\nCustomer: ${userCommand}, { model: "gpt-5-nano" })
        .then(res => {
          console.log(res);
          try {
            const jsonResponse = JSON.parse(res.message.content);
            document.getElementById("output").innerText = JSON.stringify(jsonResponse, null, 2);

            // Send JSON back to ESP32 via GET request
            const xhr = new XMLHttpRequest();
            let url = /receive?mode=${jsonResponse.mode}&action=${jsonResponse.action};
            if(jsonResponse.target) url += &target=${jsonResponse.target};
            if(jsonResponse.item) url += &item=${jsonResponse.item};
            xhr.open("GET", url, true);
            xhr.send();

          } catch (err) {
            console.error(err);
            document.getElementById("output").innerText = "Error parsing JSON:\n" + res.message.content;
          }
        })
        .catch(err => {
          console.error(err);
          document.getElementById("output").innerText = "Error contacting AI";
        });
    };

    window.startMic = function() {
      if(!('webkitSpeechRecognition' in window)) {
        alert("Speech recognition not supported!");
        return;
      }
      const recognition = new webkitSpeechRecognition();
      recognition.lang = "en-US";
      recognition.continuous = false;
      recognition.interimResults = false;

      document.getElementById("output").innerText = "Listening...";
      recognition.start();
      setTimeout(() => recognition.stop(), 20000);

      recognition.onresult = event => {
        const text = event.results[0][0].transcript;
        document.getElementById("userText").value = text;
        window.sendText();
      };

      recognition.onerror = () => {
        document.getElementById("output").innerText = "Error occurred";
      };
    };
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", webpage);
}

// Receive JSON response via query parameters
void handleReceive() {
  currentMode = server.arg("mode");
  currentAction = server.arg("action");
  targetTable = server.arg("target").toInt();
  orderedItem = server.arg("item");

  Serial.println("=== Received AI JSON ===");
  Serial.println("Mode: " + currentMode);
  Serial.println("Action: " + currentAction);
  Serial.println("Target Table: " + String(targetTable));
  Serial.println("Ordered Item: " + orderedItem);

  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/receive", handleReceive);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  // Example: Act on the AI instruction
  if(currentMode == "table_selection" && currentAction == "go_to_table") {
    // You can trigger motor code here
    Serial.println("Robot moving to table " + String(targetTable));
    currentMode = ""; // Reset after action
  }

  if(currentMode == "order_selection" && currentAction == "return_to_base") {
    // You can trigger return-to-base code
    Serial.println("Robot returning with item: " + orderedItem);
    currentMode = ""; // Reset after action
  }
  delay(100);
}