#include "simple_wifi_manager.h"

const char *firmware_version_;

Config_Wifi config_sta;
Config_Wifi config_ap;

ESP8266WebServer server(80);

const char *page_root PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>WiFi Selector</title><style>table,td,th{border:1px solid}table{border-collapse:collapse}form{margin-bottom:2rem}</style></head><body><form action=\"/update_wifi\" method=\"post\"><table><tr><th colspan=\"2\">WIFI<span id=\"wifi_status\"></span></th></tr><tr><th colspan=\"2\" id=\"ip\"></th></tr><tr><th>SSID</th><th><input type=\"text\" name=\"ssid\" id=\"ssid\" maxlength=\"49\"></th></tr><tr><th>PASS</th><th><input type=\"text\" name=\"pass\" id=\"pass\" maxlength=\"49\"></th></tr><tr><th colspan=\"2\"><button type=\"submit\">Ubah STA</button></th></tr></table></form><form action=\"/update_ap\" method=\"post\"><table><tr><th>SSID</th><th><input type=\"text\" name=\"ssid\" id=\"ap_ssid\" maxlength=\"49\"></th></tr><tr><th>PASS</th><th><input type=\"text\" name=\"pass\" id=\"ap_pass\" maxlength=\"49\"></th></tr><tr><th colspan=\"2\"><button type=\"submit\">Ubah AP</button></th></tr></table></form><form method=\"POST\" action=\"/update_firmware\" enctype=\"multipart/form-data\"><table><tr><td>Current firmware</td><td id=\"version\"></td></tr><tr><td><input type=\"file\" name=\"update\" accept=\".bin\"></td><td><button type=\"submit\" value=\"Update\">Update</button></td></tr></table></form><script>function gid(s){return document.getElementById(s)}fetch(\"/status.json\").then(s=>s.json()).then(s=>{gid(\"wifi_status\").innerText=s.connected?\" Connected\":\" Disconnected\",gid(\"ip\").innerText=s.ip,gid(\"ssid\").value=s.ssid,gid(\"pass\").value=s.pass,gid(\"version\").innerHTML=s.version,gid(\"ap_ssid\").value=s.ap_ssid,gid(\"ap_pass\").value=s.ap_pass});</script></body></html>";

void wifi_manager_loop()
{
    server.handleClient();
    MDNS.update();
}

void handle_not_found()
{
    server.send(404, "text/plain", "Not found");
}

void handle_root()
{
    server.send(200, "text/html", page_root);
}

void handle_get_status()
{
    wl_status_t wifi_status = WiFi.status();
    JsonDocument json;
    json["connected"] = wifi_status == WL_CONNECTED;
    json["version"] = firmware_version_;
    IPAddress ip_addr = IPAddress(0, 0, 0, 0);
    if (wifi_status == WL_CONNECTED)
    {
        ip_addr = WiFi.localIP();
    }
    json["ip"] = ip_addr.toString();
    json["ssid"] = config_sta.ssid;
    json["pass"] = config_sta.pass;

    json["ap_ssid"] = config_ap.ssid;
    json["ap_pass"] = config_ap.pass;
    String jsonOutput;
    serializeJson(json, jsonOutput);
    server.send(200, "application/json", jsonOutput);
}

void handle_update_wifi()
{
    if (server.hasArg("ssid") && server.hasArg("pass"))
    {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");

        strcpy(config_sta.ssid, ssid.c_str());
        strcpy(config_sta.pass, pass.c_str());
        save_config(config_sta, FS_PATH_CONFIG_STA);
    }
    server.send(200, "text/plain", "Saving config and restarting");
    delay(1000);
    ESP.restart();
}

void handle_update_ap()
{
    if (server.hasArg("ssid") && server.hasArg("pass"))
    {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");

        strcpy(config_ap.ssid, ssid.c_str());
        strcpy(config_ap.pass, pass.c_str());
        save_config(config_ap, FS_PATH_CONFIG_AP);
    }
    server.send(200, "text/plain", "Saving config and restarting");
    delay(1000);
    ESP.restart();
}

void wifi_manager_setup(
    const char *default_ap_ssid,
    const char *default_ap_pass,
    const char *default_sta_ssid,
    const char *default_sta_pass,
    const char *firmware_version,
    const char *hostname)
{
    firmware_version_ = firmware_version;
    /* Setup FS */
    bool load_config_sta = false, load_config_ap = false;
    if (LittleFS.begin())
    {
        Storage_Config config_buf;
        load_config_sta = get_config(config_buf, FS_PATH_CONFIG_STA);
        if (load_config_sta)
        {
            config_sta = config_buf.config_wifi;
        }
        load_config_ap = get_config(config_buf, FS_PATH_CONFIG_AP);
        if (load_config_ap)
        {
            config_ap = config_buf.config_wifi;
        }
    }
    else
    {
        Serial.println("Failed to mount file system");
    }
    if (!load_config_sta)
    {
        Serial.println("Using default STA WiFi");
        strcpy(config_sta.ssid, default_sta_ssid);
        strcpy(config_sta.pass, default_sta_pass);
    }
    if (!load_config_ap)
    {
        Serial.println("Using default AP WiFi");
        strcpy(config_ap.ssid, default_ap_ssid);
        strcpy(config_ap.pass, default_ap_pass);
    }

    Serial.printf("STA\t%s\t%s\n", config_sta.ssid, config_sta.pass);
    Serial.printf("AP\t%s\t%s\n", config_ap.ssid, config_ap.pass);

    /* setup wifi */
    WiFi.softAP(config_ap.ssid, config_ap.pass);
    WiFi.begin(config_sta.ssid, config_sta.pass);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    WiFi.setHostname(hostname);

    /* setup dns */
    if (MDNS.begin(hostname))
    {
        Serial.println("MDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }

    // if (LLMNR.begin(hostname))
    // {
    //     Serial.println("LLMNR responder started");
    // }

    /* setup webserver */
    server.on("/", handle_root);
    server.on("/status.json", handle_get_status);
    server.on("/update_wifi", handle_update_wifi);
    server.on("/update_ap", handle_update_ap);
    server.onNotFound(handle_not_found);

    server.on("/update_firmware", HTTP_POST, []()
              {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAIL" : "UPDATE OK");
      delay(1000);
      ESP.restart(); }, []()
              {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield(); });

    server.begin();
}

bool get_config(Storage_Config &config, const char *path)
{
    File file = LittleFS.open(path, "r");
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return false;
    }

    // Read the structure from the file
    size_t read = file.read((uint8_t *)&config, sizeof(Storage_Config));
    file.close();

    if (read != sizeof(Storage_Config))
    {
        Serial.println("Failed to read complete configuration");
        return false;
    }

    CRC32 crc;
    crc.update((uint8_t *)&config.config_wifi, sizeof(Config_Wifi));
    if (crc.finalize() != config.crc)
    {
        Serial.println("CRC mismatch: Data corrupted");
        return false;
    }

    return true;
}

bool save_config(Config_Wifi &config_wifi, const char *path)
{
    File file = LittleFS.open(path, "w");
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return false;
    }

    Storage_Config storage_config;
    storage_config.config_wifi = config_wifi;
    storage_config.crc = calculate_crc_storage_config(storage_config);

    // Write the structure to the file
    size_t written = file.write((uint8_t *)&storage_config, sizeof(Storage_Config));
    file.close();

    return written == sizeof(Storage_Config);
}

// Function to calculate CRC
uint32_t calculate_crc_storage_config(Storage_Config &storage_config)
{
    CRC32 crc;
    crc.update((uint8_t *)&storage_config, sizeof(Config_Wifi));
    return crc.finalize();
}
