/*
  EthernetWebServer.ino - Creates an ethernet webserver on an Arduino Mega or Due.
  
  Note: File names of served pages must be no more than 8 characters with no more
  than 3 character extensions (12 characters total including the extension dot).
*/

#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <FileDB.h>
#include <HTTPRequest.h>
#include <JSONConstructor.h>

#define FIELD_LENGTH 33
#define DATABASE_SIZE 10
#define DATABASE_PATH "/database.bin"

#define CONTENT_TYPES_PATH "/types.txt"
#define BAD_REQUEST_PATH "/400.htm"
#define NOT_FOUND_PATH "/404.htm"
#define HEADER_LENGTH 257
#define URI_LENGTH 129
#define PARAMS_LENGTH 129
#define COOKIES_LENGTH 65
#define JSON_LENGTH 1025
#define JSON_ELEMENT_LENGTH 65
#define FILE_PATH_LENGTH 33
#define CONTENT_TYPE_LENGTH 33
#define PORT 80
#define API_URI "/api/"
#define ROOT_PATH "/root"
#define DEFAULT_DOCUMENT "index.htm"
#define OK "200 OK"
#define BAD_REQUEST "400 Bad Request"
#define NOT_FOUND "404 Not Found"
#define JSON_TYPE "application/json"
#define FILE_TYPE "text/html"

// These structs define the type of entries to store
struct User : Entry {
  char username[FIELD_LENGTH];
  char password[FIELD_LENGTH];
  char session[FIELD_LENGTH];
  bool admin;
} user;

struct List {
  char username[FIELD_LENGTH];
  bool admin;
};

byte mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
EthernetServer server(PORT);
FileDB<User> database(DATABASE_SIZE);
HTTPRequest<HEADER_LENGTH, URI_LENGTH, PARAMS_LENGTH, COOKIES_LENGTH> request;
File content_types, bad_request, not_found;

// These are query functions passed to the database
bool queryByUsername(struct User &database_entry, struct User &query_entry) {
  return !strcmp(database_entry.username, query_entry.username);
}
bool queryByLogin(struct User &database_entry, struct User &query_entry) {
  return !strcmp(database_entry.username, query_entry.username) &&
         !strcmp(database_entry.password, query_entry.password);
}
bool queryBySession(struct User &database_entry, struct User &query_entry) {
  return !strcmp(database_entry.session, query_entry.session);
}
void listUsers(struct User &database_entry, void *data, size_t index) {
  List *list = (List *)data;
  snprintf(list[index].username, FIELD_LENGTH, "%s", database_entry.username);
  list[index].admin = database_entry.admin;
}

void newSession(char *buffer) {
  randomSeed(random(-2147483648, 2147483647) + analogRead(A0) + millis());
  for (size_t i = 0; i < FIELD_LENGTH - 1; i++) {
    snprintf(&(buffer[i]), 2, "%X", random(16));
  }
}
template<size_t T, size_t U, size_t V, size_t W, size_t X>
void apiResponse(bool success, char *message, JSON<T> &json, HTTPRequest<U, V, W, X> &request) {
  json.addBool("success", success);
  json.addString("message", message);
  request.stringResponse(OK, JSON_TYPE, json.buffer());
}

void setup() {
  Serial.begin(115200);
  
  // Turn off ready light
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  // Select the SD card
  pinMode(4, OUTPUT);
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  digitalWrite(4, LOW);

  Serial.print("Connecting to SD card...");
  if (!SD.begin(4)) {
    Serial.println(" Error!");
    return;
  }
  Serial.println(" Done.");
  
  Serial.print("Loading database...");
  if (!database.open(DATABASE_PATH)) {
    Serial.print(" Done. (Created a new database)\r\n"
    "Adding database entries...");
    snprintf(user.username, FIELD_LENGTH, "%s", "admin");
    snprintf(user.password, FIELD_LENGTH, "%s", "password");
    user.admin = true;
    newSession(user.session);
    if (!database.add(user, queryByUsername)) {
      Serial.println(" Error!");
      return;
    }
  }
  Serial.println(" Done.");
  
  Serial.print("Loading files...");
  content_types = SD.open(CONTENT_TYPES_PATH);
  bad_request = SD.open(BAD_REQUEST_PATH);
  not_found = SD.open(NOT_FOUND_PATH);
  if (!content_types || !bad_request || !not_found) {
    Serial.println(" Error!");
    return;
  }
  Serial.println(" Done.");
  
  // Select the W5100
  digitalWrite(4, HIGH);
  digitalWrite(10, LOW);
  
  Serial.print("Connecting to network...");
  if (!Ethernet.begin(mac)) {
    Serial.println(" Error!");
    return;
  }
  Serial.println(" Done.");
  
  server.begin();
  Serial.print("This server can be accessed at: ");
  Serial.print(Ethernet.localIP());
  Serial.print(":");
  Serial.println(PORT);
  Serial.println("\r\nAccess log:");
  
  // Turn on ready light
  digitalWrite(13, HIGH);
}

void loop() {
  EthernetClient client = server.available();
  
  // Wait for a client to connect
  if (client) {
    
    // Start a new request
    if (request.begin(client)) {
      char *uri;    
      
      // Print request information to the console
      switch (request.method()) {
        case GET:
          Serial.print("GET ");
          break;
        case POST:
          Serial.print("POST ");
          break;
        case HEAD:
          Serial.print("HEAD ");
          break;
      }
      Serial.print(request.uri());
      Serial.print(" [Params: ");
      Serial.print(request.params());
      Serial.print("  Cookies: ");
      Serial.print(request.cookies());
      Serial.println("]");
      
      uri = request.uri();
      
      // Process API requests
      if (strstr(uri, API_URI) == uri) {
        char *call = uri + strlen(API_URI);
        JSON<JSON_LENGTH> json;
        
        if (!strcmp(call, "login")) {
          if (request.getParam("username", user.username, FIELD_LENGTH) &&
              request.getParam("password", user.password, FIELD_LENGTH) &&
              database.get(user, queryByLogin)) {
            newSession(user.session);
            if (database.add(user, queryByUsername)) {
              json.addBool("success", true);
              json.addString("message", "Logged in.");
              json.addString("username", user.username); 
              json.addBool("admin", user.admin);
              request.stringResponse(OK, JSON_TYPE, json.buffer(), "SESSION", user.session, "/");
            } else {
              apiResponse(false, "Internal server error.", json, request);
            }
          } else if (request.getCookie("SESSION", user.session, FIELD_LENGTH) &&
                     database.get(user, queryBySession)) {
            json.addBool("success", true);
            json.addString("message", "Logged in.");
            json.addString("username", user.username); 
            json.addBool("admin", user.admin);
            request.stringResponse(OK, JSON_TYPE, json.buffer());
          } else {
            apiResponse(false, "Invalid username/password.", json, request);
          }
        } else if (request.getCookie("SESSION", user.session, FIELD_LENGTH) &&
                   database.get(user, queryBySession)) {
          if (!strcmp(call, "logout")) {
            user.session[0] = '\0';
            if (database.add(user, queryByUsername)) {
              apiResponse(true, "Logged out.", json, request);
            } else {
              apiResponse(false, "Internal server error.", json, request);
            }
          } else if (!strcmp(call, "edit") &&
                     request.getParam("username", user.username, FIELD_LENGTH) &&
                     request.getParam("password", user.password, FIELD_LENGTH)) {
            if (database.add(user, queryBySession)) {
              apiResponse(true, "Changes saved.", json, request);
            } else {
              apiResponse(false, "Internal server error.", json, request);
            }
          } else if (!strcmp(call, "toggle")) {
            json.addBool("success", true);
            if (digitalRead(13)) {
              digitalWrite(13, LOW);
              apiResponse(true, "Status LED off.", json, request);
            } else {
              digitalWrite(13, HIGH);
              apiResponse(true, "Status LED on.", json, request);
            }
          } else if (user.admin) {
            char admin[5];
            
            if (!strcmp(call, "add") &&
                request.getParam("username", user.username, FIELD_LENGTH) &&
                request.getParam("password", user.password, FIELD_LENGTH) &&
                request.getParam("admin", admin, 5)) {
              newSession(user.session);
              if (!strcmp(admin, "true")) {
                user.admin = true;
              } else {
                user.admin = false;
              }
              if (database.add(user, queryByUsername)) {
                apiResponse(true, "User added.", json, request);
              } else {
                apiResponse(false, "Internal server error.", json, request);
              }
            } else if (!strcmp(call, "list")) {
              size_t size = database.count();
              List list[size];
              JSON<JSON_ELEMENT_LENGTH> elements[size];
              
              json.addBool("success", true);
              json.addString("message", "List successful.");
              database.list(list, listUsers);
              for (size_t i = 0; i < size; i++) {
                elements[i].addString("username", list[i].username);
                elements[i].addBool("admin", list[i].admin);
              }
              json.addObjectArray("users", elements, size);
              request.stringResponse(OK, JSON_TYPE, json.buffer());
            } else if (!strcmp(call, "remove") &&
                       request.getParam("username", user.username, FIELD_LENGTH)) {
              if (database.remove(user, queryByUsername)) {
                apiResponse(false, "User removed.", json, request);
              } else {
                apiResponse(false, "Internal server error.", json, request);
              }
            } else {
              apiResponse(false, "Invalid API call.", json, request);
            }
          } else {
            apiResponse(false, "Invalid API call.", json, request);
          }
        } else {
          apiResponse(false, "Invalid API call.", json, request);
        }
      } else {
        char file_path[FILE_PATH_LENGTH];
        
        // Convert the URI to a file path
        if (uri[strlen(uri) - 1] == '/') {
          snprintf(file_path, FILE_PATH_LENGTH, "%s%s%s", ROOT_PATH, uri, DEFAULT_DOCUMENT);
        } else if (!strchr(uri, '.')) {
          snprintf(file_path, FILE_PATH_LENGTH, "%s%s/%s", ROOT_PATH, uri, DEFAULT_DOCUMENT);
        } else {
          snprintf(file_path, FILE_PATH_LENGTH, "%s%s", ROOT_PATH, uri);
        }
        
        // Decide what to return
        File file = SD.open(file_path);
        if (file) {
          size_t index = 0;
          char *extension, buffer[CONTENT_TYPE_LENGTH], *delim,
          content_type[CONTENT_TYPE_LENGTH];

          snprintf(content_type, CONTENT_TYPE_LENGTH, "text/html");
          extension = strchr(file_path, '.');
          content_types.seek(0);
          while (content_types.available()) {
            if (index == CONTENT_TYPE_LENGTH) {
              index = 0;
            }
            buffer[index++] = (char)content_types.read();
            if (buffer[index - 1] == '\n') {
              buffer[index - 1] = '\0';
              index = 0;
              delim = strchr(buffer, '\t');
              if (strstr(buffer, extension) == buffer && delim) {
                snprintf(content_type, CONTENT_TYPE_LENGTH, "%s", delim + 1);
                break;
              }
            }
          }
          request.fileResponse(OK, content_type, file);
          file.close();
        } else {
          request.fileResponse(NOT_FOUND, FILE_TYPE, not_found);
        }
      }
    } else {
      request.fileResponse(BAD_REQUEST, FILE_TYPE, bad_request);
    }
    client.stop();
  }
}
