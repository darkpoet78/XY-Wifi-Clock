#include <ESP8266WebServer.h>


void startWebServer()
{
    Serial.println("Starting Web server");

    server.on("/",                  handleRoot);
    server.on("/config",            handleConfigJson);
    server.on("/status",            handleStatusJson);
    server.on("/brightness",        handleBrightnessJson);
    server.on("/debug",             hanndleDebugText);
    server.on("/timezones.json",    handleGetTimezonesJson);
    server.on("/displayModes.json", handleGetDisplayModesJson);
    server.on("/dateFormats.json",  handleGetDateFormatsJson);
    server.on("/alarmSounds.json",  handleGetAlarmSoundsJson);
    server.on("/favicon.ico",       handleGetFavicon);
    server.onNotFound(              handleNotFound);

    server.begin();
}


// return content type and set headers based on type of file to be sent
String getContentTypeAndHeaders(String filename)
{
    String contentType;

    if (filename.endsWith(".ico") )
    {
        server.sendHeader("Cache-Control", "max-age=31536000, immutable");
        server.sendHeader("X-Content-Type-Options", "nosniff");

        contentType = "image/x-icon";
    }
    else
    {
        server.sendHeader("Cache-Control", "no-cache");
        server.sendHeader("X-Content-Type-Options", "nosniff");

        if (filename.endsWith(".html"))
        {
            contentType = "text/html; charset=utf-8";
        }

        else if (filename.endsWith(".css"))
        {
            contentType = "text/css; charset=utf-8";
        }

        else if (filename.endsWith(".json"))
        {
            contentType = "application/json; charset=utf-8";
        }

        else
        {
            contentType = "text/plain; charset = utf-8";
        }
    }

    return (contentType);
}


// handles reading a file from SPIFFS
bool handleFileRead(String path)
{
    // assume success
    bool bReturn = true;

    Serial.println("handleFileRead: " + path);

    // if a folder is requested, send the index file
    if (path.endsWith("/")) path += "index.html";

    // if the file exists as a compressed archive
    if (SPIFFS.exists(path + ".gz"))
    {
        // use the compressed verion
        path += ".gz";
    }

    // if the file exists
    if (SPIFFS.exists(path))
    {
        // get the MIME type and set headers
        String contentType = getContentTypeAndHeaders(path);

        // send the file to the client
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();

        Serial.println(String("Sent file: ") + path);
    }
    else
    {
        Serial.println(String("File Not Found: ") + path);
        bReturn = false;
    }

    return (bReturn);
}


void handleRoot()
{
    Serial.println("Web handleRoot");

    // turn off buzzer
    buzzerOn = false;

    bool notFoundError = true;

    if (handleFileRead("/Config.html"))
    {
        if (handleFileRead("/timezones.json"))
        {
            if (handleFileRead("/displayModes.json"))
            {
                if (handleFileRead("/dateFormats.json"))
                {
                    if (handleFileRead("/alarmSounds.json"))
                    {
                        notFoundError = false;
                    }
                }
            }
        }
    }

    if (notFoundError)
    {
        server.send(404, "text/plain; charset = utf-8", "404: Not Found");
    }
}


void handleConfigJson()
{
    switch (server.method())
    {
        case HTTP_GET:
            handleGetConfigJson();
            break;

        case HTTP_POST:
            handlePostConfigJson();
            break;

        default:
            server.send(405, "text/plain; charset = utf-8", "Method Not Allowed");
            break;
    }
}


void handleGetConfigJson()
{
    Serial.println("Web handleGetConfigJson");

    DynamicJsonDocument doc = convertConfigToJson();

    String buffer;
    serializeJson(doc, buffer);
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "application/json; charset=utf-8", buffer);
}


void handlePostConfigJson()
{
    Serial.println("Web handlePostConfigJson");

    String previousDeviceName     = config.getDeviceName();
    String previousTimezone       = config.getTimezone();
    String previousDisplayMode    = config.getDisplayMode();
    String previousDateFormat     = config.getDateFormat();
    String previousAlarmSound     = config.getAlarmSound();
    bool   previousAutoBrightness = config.getAutoBrightnessEnable();

    DynamicJsonDocument json(2048);

    deserializeJson(json, server.arg("plain"));
    loadSettingsFromJson(json);

    // Write it out to the serial console
    serializeJson(json, Serial);
    Serial.println();

    saveSettings();

    String deviceName     = config.getDeviceName();
    String timezone       = config.getTimezone();
    String displayMode    = config.getDisplayMode();
    String dateFormat     = config.getDateFormat();
    String alarmSound     = config.getAlarmSound();
    bool   autoBrightness = config.getAutoBrightnessEnable();

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");

    bool needReset = false;

    if (!deviceName.equalsIgnoreCase(previousDeviceName))
    {
        Serial.print("Device Name changed to: ");
        Serial.println(deviceName);
        needReset = true;
    }

    if (!timezone.equalsIgnoreCase(previousTimezone))
    {
        Serial.print("Timezone changed to: ");
        Serial.println(timezone);
        needReset = true;
    }

    if (!displayMode.equalsIgnoreCase(previousDisplayMode))
    {
        Serial.print("Display Mode changed to: ");
        Serial.println(displayMode);
    }


    if (!dateFormat.equalsIgnoreCase(previousDateFormat))
    {
        Serial.print("Date Format changed to: ");
        Serial.println(dateFormat);
    }

    if (!alarmSound.equalsIgnoreCase(previousAlarmSound))
    {
        Serial.print("Alarm Sound changed to: ");
        Serial.println(alarmSound);
    }

    if (autoBrightness != previousAutoBrightness)
    {
        Serial.print("Auto brightness changed to: ");
        Serial.println(autoBrightness ? "enabled" : "disabled");
    }

    if (needReset)
    {
        Serial.println("Config change(s) will trigger a device restart...");
        delay(1000);
        ESP.restart();
    }
}


void handleStatusJson()
{
    Serial.println("Web handleStatusJson");

    DynamicJsonDocument doc(2048);

    doc["SSID"] = WiFi.SSID();
    doc["IP"]   = WiFi.localIP().toString();
    doc["RSSI"] = WiFi.RSSI();

    char NTPbuffer[80];
    if (!updatedByNTP)
    {
        sprintf(NTPbuffer, "never");
    }
    else
    {
        unsigned long secondsSinceNTP = (millis() - last_NTP_Update) / 1000;
        unsigned long minutesSinceNTP = secondsSinceNTP / 60;
        unsigned long hoursSinceNTP   = minutesSinceNTP / 60;
        unsigned long daysSinceNTP    = hoursSinceNTP   / 24;
        secondsSinceNTP %= 60;
        minutesSinceNTP %= 60;
        hoursSinceNTP   %= 24;
        if (daysSinceNTP > 0)
        {
            sprintf(NTPbuffer, "%u dy, %u hr, %u min, %u sec",
                    (unsigned int)daysSinceNTP, (unsigned int)hoursSinceNTP, (unsigned int)minutesSinceNTP, (unsigned int)secondsSinceNTP);
        }
        else if (hoursSinceNTP > 0)
        {
            sprintf(NTPbuffer, "%u hr, %u min, %u sec",
                    (unsigned int)hoursSinceNTP, (unsigned int)minutesSinceNTP, (unsigned int)secondsSinceNTP);
        }
        else if (minutesSinceNTP > 0)
        {
            sprintf(NTPbuffer, "%u min, %u sec",
                    (unsigned int)minutesSinceNTP, (unsigned int)secondsSinceNTP);
        }
        else
        {
            sprintf(NTPbuffer, "%u sec",
                    (unsigned int)secondsSinceNTP);
        }
    }
    doc["NTP"] = NTPbuffer;

    doc["BRIGHTNESS"] = currentBrightness;

    String buffer;
    serializeJson(doc, buffer);
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "application/json; charset=utf-8", buffer);
}


void handleBrightnessJson()
{
    Serial.println("Web handleBrightnessJson");

    DynamicJsonDocument json(128);

    deserializeJson(json, server.arg("plain"));

    uint8_t newBrightness = json["newBrightness"];
    if ((newBrightness >= MIN_BRIGHTNESS)  &&  (newBrightness <= MAX_BRIGHTNESS))
    {
        manualBrightness = newBrightness;
        setDisplayBrightness(manualBrightness, true);
    }

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");
}


void hanndleDebugText()
{
    Serial.println(server.arg("plain"));

    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/plain; charset=utf-8");
}


void handleGetTimezonesJson()
{
    Serial.println("Web handleGetTimezonesJson");
    handleFileRead("/timezones.json");
}


void handleGetDisplayModesJson()
{
    Serial.println("Web handleGetDisplayModesJson");
    handleFileRead("/displayModes.json");
}


void handleGetDateFormatsJson()
{
    Serial.println("Web handleGetDateFormatsJson");
    handleFileRead("/dateFormats.json");
}


void handleGetAlarmSoundsJson()
{
    Serial.println("Web handleGetAlarmSoundsJson");
    handleFileRead("/alarmSounds.json");
}


void handleGetFavicon()
{
    Serial.println("Web handleGetFavicon");
    handleFileRead("/favicon.ico");
}


void handleNotFound()
{
    Serial.println("Web handleNotFound");

    String message = "Not Found Error\n\n";
    message += "URI: " + server.uri();
    message += "\n\n Method: " + (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\n\n Arguments: "+ server.args();
    message += "\n";

    server.send(404, "text/plain", message);
}
