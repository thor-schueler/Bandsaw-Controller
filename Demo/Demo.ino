#include <LovyanGFX.hpp>
#include <TMCStepper.h>
#include "driver/ledc.h"

// UART pins (single-wire PDN_UART)
#define TMC_UART_TX   17   // ESP32 TX → module RX pad via 1K resistor (PDN_UART)
#define TMC_UART_RX   16   // ESP32 RX → module RX pad (PDN_UART)

// Step/Dir pins
#define STEP_PIN      2
#define DIR_PIN       15
#define EN_PIN        0
#define DIAG_PIN      13

// PWM channel
#define STEP_PWM_CHANNEL 0

// TMC2209 settings
#define R_SENSE       0.10f
#define DRIVER_ADDR   0b00
#define SPEED 1500

#define SOLENOID_A_PIN 12
#define SOLENOID_B_PIN 14
#define LIGHT_STRIP_PIN 32

#define LIMIT_1_PIN 33
#define LIMIT_2_PIN 35

#define POWER_RELAY_PIN 25
#define EXPANDER_IRQ_PIN 34
#define EXPANDER_I2C_SDA_PIN 27
#define EXPANDER_I2C_SCL_PIN 26

HardwareSerial TMCSerial(2);
TMC2209Stepper driver(&TMCSerial, R_SENSE, DRIVER_ADDR);

// 10ms loop timer
unsigned long lastLoop = 0;

// Feed direction toggle timer
unsigned long lastDirToggle = 0;
bool dirState = true;
bool stall = false;
bool limit1 = false;
bool limit2 = false;
int stall_count = 0;


class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Panel_ST7796     _panel;
  lgfx::Bus_SPI          _bus;
  lgfx::Touch_XPT2046    _touch;

  LGFX() {
    // -----------------------------
    // SPI BUS (shared for display + touch)
    // -----------------------------
    {
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;     // VSPI = GPIO 18/19/23
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;      // 40 MHz
      cfg.freq_read  = 16000000;
      cfg.pin_sclk   = 18;
      cfg.pin_mosi   = 23;
      cfg.pin_miso   = 19;
      cfg.pin_dc     = 21;             // D/C pin
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    // -----------------------------
    // DISPLAY PANEL CONFIG
    // -----------------------------
    {
      auto cfg = _panel.config();
      cfg.pin_cs        = 5;          // Display CS
      cfg.pin_rst       = -1;         // Reset (or -1 if tied high)
      cfg.pin_busy      = -1;
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 0;
      cfg.dummy_read_bits  = 0;
      cfg.readable      = true;
      cfg.invert        = false;
      cfg.rgb_order     = false;
      cfg.dlen_16bit    = false;
      cfg.bus_shared    = true;       // IMPORTANT: shared SPI bus
      _panel.config(cfg);
    }

    // -----------------------------
    // TOUCH CONFIG (XPT2046)
    // -----------------------------
    {
      auto cfg = _touch.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.freq       = 2000000;       // 2 MHz
      cfg.pin_sclk   = 18;
      cfg.pin_mosi   = 23;
      cfg.pin_miso   = 19;
      cfg.pin_cs     = 4;             // Touch CS
      cfg.pin_int    = 22;            // Touch IRQ
      cfg.x_min      = 175;           // Calibrate these
      cfg.x_max      = 3800;
      cfg.y_min      = 3900;
      cfg.y_max      = 250;
      cfg.bus_shared = true;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }

    setPanel(&_panel);
  }
};

ledc_timer_config_t t = {
    .speed_mode      = LEDC_HIGH_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_12_BIT,
    .timer_num       = LEDC_TIMER_0,
    .freq_hz         = SPEED,
    .clk_cfg         = LEDC_USE_APB_CLK,
};

ledc_channel_config_t c = {
    .gpio_num   = STEP_PIN,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 512,  // 50% duty cycle on 12-bit resolution
    .hpoint     = 0
};

LGFX lcd;

void IRAM_ATTR stall_isr() {
    stall = true;
}

void IRAM_ATTR limit_1_isr() {
    limit1 = !digitalRead(LIMIT_1_PIN);
}

void IRAM_ATTR limit_2_isr() {
    limit2 = !digitalRead(LIMIT_2_PIN);
}

void setup() {
  Serial.begin(115200);
  pinMode(4, INPUT_PULLUP);

  lcd.init();
  lcd.setRotation(1);   // Landscape
  lcd.fillScreen(TFT_BLACK);

  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.println("ST7796S + XPT2046 Test");
  lcd.println("Touch the screen...");

  // UART init
  TMCSerial.begin(115200, SERIAL_8N1, TMC_UART_RX, TMC_UART_TX);
  Serial.begin(115200);

  // Stepper pins
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);

  // -----------------------------
  // Hardware PWM for STEP
  // -----------------------------
  ledc_timer_config(&t);
  ledc_channel_config(&c);
  //ledcAttach(STEP_PIN, 200, 12);
  //ledcWrite(STEP_PIN, 2048);
  //ledcAttachChannel(STEP_PIN, 2000, 12, STEP_PWM_CHANNEL);
  //ledcWriteChannel(STEP_PWM_CHANNEL, 2048);
  // 2000 Hz STEP frequency
  // 128 = 50% duty on 8-bit resolution

  // Driver init
  driver.begin();
  driver.toff(4);
  driver.blank_time(24);
  driver.rms_current(2000);
  driver.microsteps(4);
  driver.pwm_autoscale(true);   // StealthChop

  // -----------------------------
  // StallGuard configuration
  // -----------------------------
  driver.en_spreadCycle(true);  // StallGuard ONLY works in SpreadCycle
  driver.pwm_autoscale(false);  // Disable StealthChop
  driver.TCOOLTHRS(0xFFFFF);    // Allow SG to operate at all speeds
  driver.SGTHRS(50);            // StallGuard threshold (tune this)
  driver.irun(31);
  driver.ihold(31);

  //uint32_t cool = 0;
  //cool |= (2 << 0);   // SEMIN = 2 (CoolStep enabled)
  //cool |= (4 << 5);   // SEMAX = 4
  //cool |= (1 << 8);   // SEUP = 1
  //cool |= (1 << 10);  // SEDN = 1
  //driver.COOLCONF(cool);
  //driver.COOLCONF(0); 

  pinMode(DIAG_PIN, INPUT_PULLUP);
  attachInterrupt(DIAG_PIN, stall_isr, RISING);  // or FALLING depending on polarity

  pinMode(LIMIT_1_PIN, INPUT);
  pinMode(LIMIT_2_PIN, INPUT);
  attachInterrupt(LIMIT_1_PIN, limit_1_isr, CHANGE);  
  attachInterrupt(LIMIT_2_PIN, limit_2_isr, CHANGE);  

  lcd.println("TMC2209 initialized with StallGuard.");

  pinMode(SOLENOID_A_PIN, OUTPUT);
  pinMode(SOLENOID_B_PIN, OUTPUT);
  pinMode(LIGHT_STRIP_PIN, OUTPUT);
  digitalWrite(SOLENOID_A_PIN, LOW);
  digitalWrite(SOLENOID_B_PIN, LOW);
  digitalWrite(LIGHT_STRIP_PIN, LOW);
  Serial.println("Setup complete.");

  Serial.println(driver.IHOLD_IRUN(), HEX);
  Serial.println(driver.TPOWERDOWN(), HEX);
  Serial.println(driver.GCONF(), HEX);
  Serial.println(driver.DRV_STATUS(), HEX);


  pinMode(POWER_RELAY_PIN, OUTPUT);
  pinMode(EXPANDER_IRQ_PIN, INPUT);
  digitalWrite(POWER_RELAY_PIN, HIGH); 
}

uint16_t speed=SPEED;
void loop() {
  uint16_t x, y;
  unsigned long now = millis();

  // -----------------------------
  // Toggle direction every 3 seconds
  // -----------------------------
  if (now - lastDirToggle >= 3000) {
    lastDirToggle = now;
    dirState = !dirState;
    digitalWrite(DIR_PIN, dirState ? HIGH : LOW);
    digitalWrite(SOLENOID_A_PIN, dirState ? HIGH : LOW);
    digitalWrite(SOLENOID_B_PIN, dirState ? LOW : HIGH); 
    digitalWrite(LIGHT_STRIP_PIN, dirState ? LOW : HIGH);   
    digitalWrite(POWER_RELAY_PIN, dirState ? LOW : HIGH);  
    Serial.print("."); 
  }

// -----------------------------
  // 10ms control loop
  // -----------------------------
  if (now - lastLoop >= 10) {
    lastLoop = now;

    //if(speed < SPEED*2)
    //{
    //    speed += 1;
    //    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, speed);
    //    lcd.setCursor(0, 160);
    //    lcd.fillRect(0, 160, 200, 40, TFT_BLACK);
    //    lcd.printf("Speed: %d", speed);
    //}

    lcd.setCursor(0, 280);
    lcd.fillRect(0, 280, 200, 40, TFT_BLACK);
    if(stall) lcd.print("Stall detected");
    if(stall && (++stall_count) > 100){
        stall = false;
        stall_count = 0;
    } 

    // Read StallGuard load
    uint16_t load = driver.SG_RESULT();
    lcd.setCursor(0, 240);
    lcd.fillRect(0, 240, 200, 40, TFT_BLACK);
    lcd.printf("Load: %d", load);

    // Adjust feed rate based on load
    if (load < 200) {
      ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, SPEED * 0.75);
    } 
    else if (load > 220) {
        ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, SPEED);
    }

    // -----------------------------
    // Your other 10ms tasks go here
    // -----------------------------
    if (lcd.getTouch(&x, &y)) {
        lcd.fillCircle(x, y, 5, TFT_RED);

        lcd.setCursor(0, 200);
        lcd.fillRect(0, 200, 200, 40, TFT_BLACK);
        lcd.printf("Touch: %d, %d", x, y);
    }

    if(limit1 || limit2){
        lcd.setCursor(0, 160);
        lcd.fillRect(0, 160, 200, 40, TFT_BLACK);
        lcd.printf("Limit: %d, %d", limit1, limit2);
    }
    else{
        lcd.setCursor(0, 160);
        lcd.fillRect(0, 160, 200, 40, TFT_BLACK);
    }
  }
  delay(5);
}
