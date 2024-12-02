#include "simple_wifi_manager.h"

Config_Wifi config_sta;
Config_Wifi config_ap;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
WiFiEventHandler wifiAPhandler;

ESP8266WebServer server(80);

const char *page_root PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>WiFi Selector</title><style>table,td,th{border:1px solid}table{border-collapse:collapse}</style></head><body><form action=\"/update_wifi\" method=\"post\"><table><tr><th colspan=\"2\">WiFi <span id=\"wifi_status\"></span></th></tr><tr><th colspan=\"2\">Set WiFi</th></tr><tr><th>SSID</th><th><input type=\"text\" name=\"ssid\" id=\"ssid\" maxlength=\"49\"></th></tr><tr><th>PASS</th><th><input type=\"text\" name=\"pass\" id=\"pass\" maxlength=\"49\"></th></tr><tr><th colspan=\"2\"><button type=\"submit\">Simpan</button></th></tr></table></form><script>function gid(e){return document.getElementById(e)}fetch(\"/status.json\").then(e=>e.json()).then(e=>{gid(\"wifi_status\").innerText=e.connected?\"Connected\":\"Disconnected\",gid(\"ssid\").value=e.ssid,gid(\"pass\").value=e.pass});</script></body></html>";

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
    JsonDocument json;
    json["connected"] = WiFi.status() == WL_CONNECTED;
    json["ssid"] = config_sta.ssid;
    json["pass"] = config_sta.pass;
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
    // server.sendHeader("Location", "/", true);
    // server.send(302, "text/plain", "");
    server.send(200, "text/plain", "Saving config and restarting");
    delay(1000);
    ESP.restart();
}

void wifi_manager_setup(char *default_ap_ssid, char *default_ap_pass, char *default_sta_ssid, char *default_sta_pass)
{
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
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
    wifiAPhandler = WiFi.onSoftAPModeStationConnected(onSoftApConnected);
    WiFi.softAP(config_ap.ssid, config_ap.pass);
    WiFi.begin(config_sta.ssid, config_sta.pass);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    /* setup dns */
    if (MDNS.begin("esp8266"))
    {
        Serial.println("MDNS responder started");
    }

    /* setup webserver */
    server.on("/", handle_root);
    server.on("/status.json", handle_get_status);
    server.on("/update_wifi", handle_update_wifi);
    server.onNotFound(handle_not_found);
    server.begin();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
    Serial.printf("Connected to Wi-Fi %s\n", config_sta.ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
    Serial.println("Disconnected from Wi-Fi");
}

void onSoftApConnected(const WiFiEventSoftAPModeStationConnected &event)
{
    Serial.println("SoftAP connected");
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
