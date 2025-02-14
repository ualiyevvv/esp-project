#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

#include <Wire.h>                // Для работы с I2C
#include <U8g2lib.h>             // Библиотека для работы с дисплеем SSD1306

#include <time.h>

WiFiManager wifiManager;
Preferences preferences;
AsyncWebServer server(80);

#define LED_RED 2   // Красный светодиод
#define LED_GREEN 4 // Зелёный светодиод
#define RESET_BTN 0 // Кнопка сброса WiFi

#define DHT_PIN 5        // Пин, к которому подключён датчик KY-015
#define DHT_TYPE DHT11   // Тип датчика (DHT11 или DHT22)

#define BUZZER_PIN 17

DHT dht(DHT_PIN, DHT_TYPE);

// Инициализация дисплея с использованием I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


bool alarmActive = false;

unsigned long lastUpdate = 0; // для сверки с реальной секундой
int speedFactor = 5; // если равно 1, то как 1с==1мин
int hours = 23;
int minutes = 34;
int seconds = 0;

void updateTime() {
  seconds += speedFactor; // Увеличиваем секунды с учётом ускорения времени

  if (seconds >= 60) {
    seconds = 0;
    minutes++;

    if (minutes >= 60) {
      minutes = 0;
      hours++;

      if (hours >= 24) {
          hours = 0;
      }
    }
  }
}



void displayTime(int number) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB18_tr);

  // Разбираем число на часы и минуты
  int hours = number / 100;
  int minutes = number % 100;

  // Формируем строку времени HH:MM
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", hours, minutes);

  u8g2.drawStr(20, 30, timeStr);
    
  // Serial.print("Текущее время: ");
  // Serial.printf("%02d:%02d:%02d (x%.1f)\n", hours, minutes, speedFactor);

  u8g2.sendBuffer();
}


void syncTimeWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("❌ Нет подключения к интернету! Время не обновлено.");
      return;
  }

  configTime(5 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // GMT+5
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
      Serial.println("❌ Ошибка: не удалось получить время с NTP!");
      return;
  }

  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  speedFactor = 1;  // Сбрасываем ускорение
  Serial.printf("✅ Время синхронизировано: %02d:%02d\n", hours, minutes);
}


// Инициализация дисплея
void setupDisplay() {

  Serial.println("⚠️ Дисплей start!");
  // Попытка инициализации дисплея
  if (!u8g2.begin()) {
    // Если инициализация не удалась
    Serial.println("❗Ошибка: Не удалось подключить дисплей!");
  } else {
    // Если дисплей подключился успешно
    Serial.println("✅ Дисплей успешно подключен!");
    
    // Отображение текста на экране
    u8g2.clearBuffer();  // Очистка буфера
    u8g2.setFont(u8g2_font_ncenB08_tr);  // Установка шрифта
    u8g2.drawStr(0, 10, "Hello, ESP32!");  // Выводим текст на экран
    u8g2.sendBuffer();  // Отправка данных на экран
  }
  Serial.println("⚠️ Дисплей end!");

}


void setLEDState(bool isConnected) {
    digitalWrite(LED_RED, !isConnected);
    digitalWrite(LED_GREEN, isConnected);
}

void resetWiFiSettings() {
    Serial.println("🔥 Сброс только WiFi настроек...");
    
    // Удаляем только сохранённые SSID и пароль
    preferences.remove("ssid");
    preferences.remove("password");

    // Сбрасываем настройки WiFiManager
    wifiManager.resetSettings();

    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);

    delay(3000);
    ESP.restart();
}

void connectToWiFi() {
    Serial.println("📡 Подключение к WiFi...");

    if (!wifiManager.autoConnect("ESP32_Setup", "12345678")) {
        Serial.println("❌ Не удалось подключиться, перезапуск...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("✅ Подключено к WiFi!");
    Serial.print("🌐 IP-адрес: ");
    Serial.println(WiFi.localIP());

    preferences.putString("ssid", WiFi.SSID());
    preferences.putString("password", WiFi.psk());

    setLEDState(true);
}

void checkAlarms() {
    preferences.begin("alarms", true);
    for (int i = 0; i < 10; i++) {
        String key = "alarm_" + String(i);
        String value = preferences.getString(key.c_str(), "");
        if (value != "") {
            int alarmHour = value.substring(0, 2).toInt();
            int alarmMinute = value.substring(3, 5).toInt();

            if (alarmHour == hours && alarmMinute == minutes && !alarmActive) {
                Serial.printf("🔔 Будильник сработал: %02d:%02d\n", alarmHour, alarmMinute);
                startAlarm();
            }
        }
    }
    preferences.end();
}

// Сохранение будильника
void saveAlarm(int id, int hour, int minute) {
    String key = "alarm_" + String(id);
    String value = String(hour) + ":" + String(minute);
    preferences.putString(key.c_str(), value);
    Serial.println("✅ Будильник сохранён: " + value);
}

// Загрузка будильников
void loadAlarms() {
    Serial.println("⏰ Загруженные будильники:");
    for (int i = 0; i < 5; i++) { // Максимум 5 будильников
        String key = "alarm_" + String(i);
        String value = preferences.getString(key.c_str(), "");
        if (value != "") {
            Serial.println("🔔 Будильник #" + String(i) + ": " + value);
        }
    }
}


void startAlarm() {
    alarmActive = true;
    digitalWrite(BUZZER_PIN, HIGH);
}

void stopAlarm() {
    alarmActive = false;
    digitalWrite(BUZZER_PIN, LOW);
}

void setupDataEndpoint() {
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Здесь пример с заглушками, нужно заменить на реальные данные с датчика
        float temperature = 23.5; // Примерное значение (замени на данные с DHT)
        float humidity = 50.0;    // Примерное значение (замени на данные с DHT)

        String json = "{";
        json += "\"temperature\": " + String(temperature, 1) + ",";
        json += "\"humidity\": " + String(humidity, 1);
        json += "}";

        request->send(200, "application/json", json);
    });
}


void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String page = R"rawliteral(
        <!DOCTYPE html>
        <html lang="ru">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <title>ESP32 Данные</title>
            <style>
                body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }
                h2 { color: green; }
                .data-box { font-size: 1.5em; margin: 20px; }
                .alarm-box { margin-top: 20px; }
                .alarm-item { margin: 5px; padding: 10px; border: 1px solid #ccc; display: inline-block; }
                .delete-btn { color: red; cursor: pointer; margin-left: 10px; }
                .form-container { margin-top: 20px; }
            </style>
            <script>
                function fetchData() {
                  fetch('/data')
                    .then(response => response.json())
                    .then(data => {
                        document.getElementById("temp").innerText = data.temperature + "°C";
                        document.getElementById("hum").innerText = data.humidity + "%";
                    })
                    .catch(error => console.error('Ошибка загрузки данных:', error));

                  fetch('/time')
                  .then(response => response.text())
                  .then(time => {
                      document.getElementById("time").innerText = time;
                  })
                  .catch(error => console.error('Ошибка загрузки времени:', error));
                }
                function fetchAlarms() {
                  fetch('/alarms')
                    .then(response => response.json())
                    .then(alarms => {
                        let alarmList = document.getElementById("alarms");
                        alarmList.innerHTML = "";
                        alarms.forEach((alarm, index) => {
                            alarmList.innerHTML += `<div class='alarm-item'>
                                ⏰ ${alarm} <span class='delete-btn' onclick='deleteAlarm(${index})'>❌</span>
                            </div>`;
                        });
                    })
                    .catch(error => console.error('Ошибка загрузки будильников:', error));
                }
                function deleteAlarm(index) {
                  fetch(`/delete_alarm?id=${index}`)
                      .then(() => fetchAlarms());
                }
                function addAlarm() {
                  let time = document.getElementById("alarmTime").value;
                  if (time) {
                    fetch(`/add_alarm?time=${time}`)
                      .then(() => {
                          fetchAlarms();
                          document.getElementById("alarmTime").value = "";
                      });
                  }
                }
                
                function setTime() {
                  let hour = document.getElementById("hour").value;
                  let minute = document.getElementById("minute").value;
                  if (hour !== "" && minute !== "") {
                    fetch(`/set_time?hour=${hour}&minute=${minute}`)
                      .then(() => alert("Время обновлено!"));
                  }
                }

                function setSpeed() {
                  let speed = document.getElementById("speed").value;
                  if (speed !== "") {
                    fetch(`/set_speed?speed=${speed}`)
                      .then(() => alert("Коэффициент ускорения обновлён!"));
                  }
                }

                function syncTime() {
                  fetch('/sync_time')
                    .then(response => response.text())
                    .then(text => alert(text))
                    .catch(error => alert("Ошибка связи с сервером ESP32!"));
                }

                function clearAlarms() {
                  fetch('/clear_alarms')
                    .then(response => response.text())
                    .then(data => {
                        alert(data === "CLEARED" ? "Будильники удалены" : "Ошибка");
                    });
                }

                setInterval(fetchData, 1000);
                setInterval(fetchAlarms, 1000);
                window.onload = () => { fetchData(); fetchAlarms(); };
            </script>
        </head>
        <body>
            <h2>✅ ESP32 подключен к WiFi!</h2>
            <p>IP-адрес: <b>)rawliteral" + WiFi.localIP().toString() + R"rawliteral(</b></p>
            <div class="data-box">
                ⏰ Время: <span id="time">Загрузка...</span><br>
                <button onclick="syncTime()">Синхронизировать время</button><br>
                🌡 Температура: <span id="temp">Загрузка...</span><br>
                💧 Влажность: <span id="hum">Загрузка...</span>
            </div>
            <div class="alarm-box">
                <h3>Будильники:</h3>
                <div id="alarms">Загрузка...</div>
                <div class="form-container">
                    <h3>Добавить будильник</h3>
                    <input type="time" id="alarmTime">
                    <button onclick="addAlarm()">Добавить</button>
                </div>

                </br>
                <button onclick="clearAlarms()">Очистить будильники</button>

                <div class="form-container">
                  <h3>Настройка времени</h3>
                  <input type="number" id="hour" placeholder="Часы" min="0" max="23">
                  <input type="number" id="minute" placeholder="Минуты" min="0" max="59">
                  <button onclick="setTime()">Установить</button>
                </div>

                <div class="form-container">
                  <h3>Настройка ускорения времени</h3>
                  <input type="number" id="speed" placeholder="x1" min="1">
                  <button onclick="setSpeed()">Установить</button>
              </div>
            </div>
        </body>
        </html>
    )rawliteral";

    request->send(200, "text/html; charset=UTF-8", page);
  });

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", hours, minutes);
    request->send(200, "text/plain", timeStr);
  });

  // Установка времени через веб-интерфейс
  server.on("/set_time", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("hour") && request->hasParam("minute")) {
        hours = request->getParam("hour")->value().toInt();
        minutes = request->getParam("minute")->value().toInt();
        seconds = 0; // Сбрасываем секунды при установке нового времени

        request->send(200, "text/plain", "OK");
        Serial.printf("🕒 Установлено время: %02d:%02d\n", hours, minutes);
    } else {
        request->send(400, "text/plain", "Ошибка: Укажите hour и minute");
    }
  });

  // Установка коэффициента ускорения времени
  server.on("/set_speed", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("speed")) {
        speedFactor = request->getParam("speed")->value().toInt();
        request->send(200, "text/plain", "OK");
        Serial.printf("⏩ Новый коэффициент ускорения: x%d\n", speedFactor);
    } else {
        request->send(400, "text/plain", "Ошибка: Укажите speed");
    }
  });

  server.on("/sync_time", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (WiFi.status() != WL_CONNECTED) {
        request->send(500, "text/plain", "Ошибка: нет подключения к интернету!");
    } else {
        syncTimeWithNTP();
        request->send(200, "text/plain", "Время успешно синхронизировано!");
    }
  });



  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
      float temp = dht.readTemperature();
      float hum = dht.readHumidity();
      String json = "{\"temperature\": " + String(temp, 1) + ", \"humidity\": " + String(hum, 1) + "}";
      request->send(200, "application/json", json);
  });

  
  server.on("/alarms", HTTP_GET, [](AsyncWebServerRequest *request) {
    String alarmsJSON = "[";
    preferences.begin("alarms", true);
    for (int i = 0; i < 10; i++) {
        String key = "alarm_" + String(i);
        String value = preferences.getString(key.c_str(), "");
        if (value != "") {
            if (alarmsJSON.length() > 1) alarmsJSON += ",";
            alarmsJSON += "\"" + value + "\"";
        }
    }
    preferences.end();
    alarmsJSON += "]";
    request->send(200, "application/json", alarmsJSON);
  });

  server.on("/delete_alarm", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
        int id = request->getParam("id")->value().toInt();
        String key = "alarm_" + String(id);
        preferences.begin("alarms", false);
        preferences.remove(key.c_str());
        preferences.end();
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/add_alarm", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time")) {
      String newAlarm = request->getParam("time")->value();

      preferences.begin("alarms", false);
      
      // Проверяем, существует ли уже такое время
      bool exists = false;
      for (int i = 0; i < 10; i++) {
          String key = "alarm_" + String(i);
          String value = preferences.getString(key.c_str(), "");
          if (value == newAlarm) {  // Если уже есть такое время, выходим
              exists = true;
              break;
          }
      }

      // Если время не найдено, добавляем
      if (!exists) {
          for (int i = 0; i < 10; i++) {
              String key = "alarm_" + String(i);
              String value = preferences.getString(key.c_str(), "");
              if (value == "") {  // Найти пустое место
                  preferences.putString(key.c_str(), newAlarm);
                  break;
              }
          }
      }

      preferences.end();
      request->send(200, "text/plain", exists ? "ALREADY_EXISTS" : "OK");
    } else {
      request->send(400, "text/plain", "MISSING_PARAM");
    }
  });

  server.on("/clear_alarms", HTTP_GET, [](AsyncWebServerRequest *request) {
    preferences.begin("alarms", false);
    for (int i = 0; i < 10; i++) {
        String key = "alarm_" + String(i);
        preferences.remove(key.c_str()); // Преобразуем String в const char*
    }
    preferences.end();
    request->send(200, "text/plain", "CLEARED");
  });


  server.begin();
}


void handleResetButton() {
    static unsigned long pressStartTime = 0;
    static bool isPressed = false;

    if (digitalRead(RESET_BTN) == LOW) {
        if (!isPressed) {
            pressStartTime = millis();
            isPressed = true;
        }
        if (millis() - pressStartTime >= 5000) { // 5 секунд удержания
            resetWiFiSettings();
        }
    } else {
        isPressed = false;
    }
}

void readTemperatureAndHumidity(float &temperature, float &humidity) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("❌ Ошибка чтения с датчика KY-015!");
        temperature = 0.0;
        humidity = 0.0;
    } else {
        Serial.print("🌡 Температура: ");
        Serial.print(temperature);
        Serial.print("°C  |  💧 Влажность: ");
        Serial.print(humidity);
        Serial.println("%");
    }
}


void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== 🚀 ESP32 ЗАПУСК ===");

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);


  setLEDState(false);

  dht.begin();

  loadAlarms();


  setupDisplay();
  //saveAlarm(0, 7, 30);
  //saveAlarm(1, 8, 00);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("🔄 Причина перезагрузки: ");
  Serial.println(reason);

  preferences.begin("wifi", false);

  connectToWiFi();
  setupWebServer();
  setupDataEndpoint();

  

}

void handleAlarmButton() {
    if (digitalRead(RESET_BTN) == LOW && alarmActive) { // Если кнопка нажата и будильник активен
        Serial.println("🛑 Будильник отключен кнопкой!");
        stopAlarm();
        delay(500); // Антидребезг кнопки
    }
}

void loop() {
  handleResetButton();

  unsigned long currentTime = millis();
  // сверка с реальной секундой
  // spead factor для ускорения или замедления
  if (currentTime - lastUpdate >= 1000) { // Реальная секунда прошла
    lastUpdate = currentTime;
    updateTime();
  }

  // display clock hh:mm
  int timeToDisplay = hours * 100 + minutes;
  displayTime(timeToDisplay);
  
  handleAlarmButton();
  
  // Проверяем будильник только раз в минуту
  static int lastCheckedMinute = -1;
  if (minutes != lastCheckedMinute) {
    checkAlarms();
    lastCheckedMinute = minutes;
  }

  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime >= 5000) { // Считываем раз в 5 секунд
    lastReadTime = millis();
    float temp, hum;
    readTemperatureAndHumidity(temp, hum);
  } 


}
