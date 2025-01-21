#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "PORSCHE";
const char* password = "kinnsporsche";

// Gemini API Token and Settings
const char* Gemini_Token = "AIzaSyC_OMNC2wV4zjMx0KRJoUmDOc1N9B3QApg";
const char* Gemini_Max_Tokens = "100"; 

// Create a web server object that listens for HTTP requests on port 80
WebServer server(80);

// To store question and answer
String question = "";
String answer = "";

// LED and Speaker Pins
#define LED_PIN 2
#define SPEAKER_PIN 32

// HTML page with improved styling
String getStyledHTMLPage(String question, String answer) {
  String page = "<!DOCTYPE html><html><head><title>Question Answer App</title>";
  page += "<style>";
  page += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f9; color: #333; }";
  page += "header { background-color: #4CAF50; padding: 20px; text-align: center; color: white; }";
  page += "main { display: flex; justify-content: center; align-items: center; height: 100vh; }";
  page += "form { background-color: white; padding: 40px; border-radius: 8px; box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1); }";
  page += "h1 { font-size: 24px; }";
  page += "label, input[type=text], input[type=submit] { display: block; width: 100%; margin: 10px 0; font-size: 16px; }";
  page += "input[type=text] { padding: 10px; border: 1px solid #ccc; border-radius: 4px; }";
  page += "input[type=submit] { background-color: #4CAF50; color: white; border: none; padding: 12px 20px; cursor: pointer; border-radius: 4px; }";
  page += "input[type=submit]:hover { background-color: #45a049; }";
  page += "p { font-size: 18px; margin-top: 20px; }";
  page += "</style></head><body>";
  
  page += "<header><h1>TEXT TO SPEECH USING ESP32</h1></header>";
  page += "<main>";
  page += "<form action=\"/ask\" method=\"POST\">";
  page += "<label for=\"question\">Ask Your Question:</label>";
  page += "<input type=\"text\" id=\"question\" name=\"question\" placeholder=\"Type your question here...\" value=\"" + question + "\">";
  page += "<input type=\"submit\" value=\"Submit\">";
  
  if (answer != "") {
    page += "<p><strong>Answer:</strong> " + answer + "</p>";
  }

  page += "</form>";
  page += "</main></body></html>";
  
  return page;
}

// Function to handle the root URL ("/") and serve the input form
void handleRoot() {
  server.send(200, "text/html", getStyledHTMLPage("", ""));  // Serve the form initially
}

// Function to handle the question submission
void handleAsk() {
  // Get the question from the POST request
  if (server.hasArg("question")) {
    question = server.arg("question");
    question.trim();
    
    // Check for "LED ON" or "LED OFF" command in the question
    if (question.equalsIgnoreCase("LED ON")) {
      digitalWrite(LED_PIN, HIGH);
      answer = "LED is turned ON";
      server.send(200, "text/html", getStyledHTMLPage(question, answer));
      return;
    } else if (question.equalsIgnoreCase("LED OFF")) {
      digitalWrite(LED_PIN, LOW);
      answer = "LED is turned OFF";
      server.send(200, "text/html", getStyledHTMLPage(question, answer));
      return;
    }

    // Fetch the answer from Gemini API if no LED command is found
    if (fetchAnswerFromAPI()) {
      // Check if answer contains "LED ON" or "LED OFF"
      if (answer.indexOf("LED ON") >= 0) {
        digitalWrite(LED_PIN, HIGH);  // Turn LED on if "LED ON" is in the answer
      } else if (answer.indexOf("LED OFF") >= 0) {
        digitalWrite(LED_PIN, LOW);   // Turn LED off if "LED OFF" is in the answer
      }

      // Play answer as tones
      playAnswerAsTone(answer);
      server.send(200, "text/html", getStyledHTMLPage(question, answer));
    } else {
      server.send(500, "text/html", "<h2>Error fetching answer!</h2>");
    }
  } else {
    server.send(400, "text/html", "<h2>No question received!</h2>");
  }
}

// Function to make the request to the Gemini API and get the response
bool fetchAnswerFromAPI() {
  HTTPClient https;

  // Prepare the Gemini API request
  if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + (String)Gemini_Token)) {
    https.addHeader("Content-Type", "application/json");
    
    // Create JSON payload with the question
    String payload = String("{\"contents\": [{\"parts\":[{\"text\":\"") + question + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": " + (String)Gemini_Max_Tokens + "}}";
    
    int httpCode = https.POST(payload);  // Send the POST request

    // Check if the request was successful
    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, response);

      // Get the answer from the JSON response
      answer = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      answer.trim();  // Trim whitespace and newlines

      https.end();
      return true;  // Successfully got the answer
    } else {
      Serial.printf("[HTTPS] POST failed, error: %s\n", https.errorToString(httpCode).c_str());
      https.end();
      return false;  // Failed to get answer
    }
  } else {
    Serial.println("Failed to connect to Gemini API");
    return false;
  }
}

// Function to play answer as tones using speaker
void playAnswerAsTone(String text) {
  int duration = 200;  // Duration of each tone in milliseconds

  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (isalnum(c)) {
      int freq = 440 + ((c - 'A') * 10);  // Generate a frequency based on the character
      tone(SPEAKER_PIN, freq, duration);  // Play the tone on the speaker
      delay(duration);  // Delay for the tone duration
    } else if (isspace(c)) {
      delay(duration);  // Add a pause for spaces
    }
  }
  noTone(SPEAKER_PIN);  // Stop the tone after playing
}

void setup() {
  Serial.begin(115200);

  // Set LED and speaker pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup server routes
  server.on("/", handleRoot);         // Serve the input form on the root URL
  server.on("/ask", HTTP_POST, handleAsk);  // Handle form submission via POST

  // Start the server
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();  // Handle incoming client requests
}
