/*
  Note that due to memory restrictions, this sketch is unstable
  on an Arduino Uno when compiled using v0.1 of HTTPRequest and
  v0.1 of FileDB.
  
  File names of served pages must be less than 8 characters with
  no more than 3 character extensions (12 characters total
  including the extension dot).
*/
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <HTTPRequest.h>
#include <FileDB.h>

#define PORT 80 // The port the server binds to
#define ROOT "/root" // The directory containing the files to serve

typedef char Key; // Defines the data type of the key

/*
  This function is used for customizing how the key is matched.
  
  Since the Arduino compiler is broken (i.e. you can't use a
  typedef as a function argument), you must change the data
  types of the arguments to match that of the key.
*/
bool keysMatch(char *a, char *b) { return !strcmp(a, b); }

/*
  Defines the type of records to store. At least 1 field must be
  named "key". The key field is used for getting and removing items.
*/
struct User : Record
{
    Key key[33];
    char password[33];
    char session[11];
} user;

byte mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; // Replace with the MAC of your shield
EthernetServer server(PORT); // Create a server instance
FileDB<User, Key> db(3, &keysMatch); // Create a database instance

void setup()
{
  Serial.begin(115200); // Set to your favorite baud
  pinMode(10, OUTPUT); // Required when using an Ethernet shield
  digitalWrite(10, HIGH); // Use only for Mega boards
  
  // Initialize the SD card library
  Serial.print("Initializing storage...");
  if (!SD.begin(4)) {
    Serial.println(" Error");
    return;
  }
  Serial.println(" Done");
  
  // Initialize the database
  Serial.print("Creating database...");
  if (!db.begin("user.db"))
  {
    Serial.println(" Error");
    return;
  }
  Serial.println(" Done");
  
  // Create a new user
  Serial.print("Adding user to database..");
  strcpy(user.key, "admin");
  strcpy(user.password, "password");
  if (!db.add(user))
  {
    Serial.println(" Error");
    return;
  }
  Serial.println(" Done");
  
  // Set the IP address using DHCP
  Serial.print("Setting IP address...");
  if (!Ethernet.begin(mac))
  {
    Serial.println(" Error");
    return;
  }
  Serial.println(" Done");
  
  server.begin();
  Serial.print("This server can be accessed at: ");
  Serial.print(Ethernet.localIP());
  Serial.print(":");
  Serial.println(PORT);
  Serial.println();
}

void loop()
{
  EthernetClient client = server.available();
  
  // Wait for a client to connect
  if (client)
  { 
    HTTPRequest request(client); // Parse an HTTP request
    bool sendData = true;
    String filePath;
    File file;
    
    // Decide which method was used (if any)
    switch (request.getMethod())
    {
      case GET:
        Serial.print("GET ");
        break;
      case POST:
        Serial.print("POST ");
        break;
      case HEAD:
        Serial.print("HEAD ");
        sendData = false;
        break;
      case UNKNOWN:
        request.response(NOT_IMPLEMENTED, 0);
        client.stop();
        return;
    }
    
    Serial.println(request.getURI());
    
    // Convert the URI to a file path
    filePath = request.getURI();
    filePath = ROOT + filePath;
    if (filePath.endsWith("/"))
    {
      filePath += "index.htm";
    }
    char temp[filePath.length() + 1];
    filePath.toCharArray(temp, filePath.length() + 1);
    
    // Try to open the file decide what to return
    file = SD.open(temp);
    if (file && !file.isDirectory())
    {
      request.response(OK, file.size());
      if (sendData)
      {
        // Relay the file 1 byte at a time (saves memory)
        while (file.available())
        {
          client.write((char)file.read());
        }
      }
      file.close();
    }
    else
    {
      request.response(NOT_FOUND, 0);
    }
    
    client.stop(); // Close the connection
  }
}
