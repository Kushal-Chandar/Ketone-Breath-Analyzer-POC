#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define ACETONE_ADC_PIN A1

// Sensor constants
const float VB_AIR = 0.295; // Vb(Air) baseline = 0.295V (295mV)
const float MIN_PPM = 0.0;  // Min PPM value
const float MAX_PPM = 20.0; // Max PPM value

// Calibration data: ADC values mapped to Acetone PPM
const int calibration_points = 6;
const int adc_deltas[] = {0, 30, 126, 200, 278, 348};
// const int adc_deltas[] = {0, 30, 340, 388, 420, 532};
const float ppm_values[] = {0, 1, 5, 10, 15, 20};

int lowestADC = 1023; // 10 bit adc, 2^10 - 1, is max adc
int highestADC = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup()
{
  Serial.begin(9600);

  pinMode(ACETONE_ADC_PIN, INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.setRotation(1);
  display.setTextWrap(true);
  display.clearDisplay();
  display.display();

  Serial.println(F("Keto Breath Analyzer Started"));
  Serial.println(F("ADC\tVb(Gas)\tΔVOUT(mV)\tPPM"));
}

// Function to convert ADC value to PPM using square root interpolation (match sensor graph)
float adcToPPM(int adc_reading)
{
  // Ensure the adc_reading is within the valid range
  if (adc_reading <= adc_deltas[0])
  {
    return ppm_values[0]; // 0 PPM
  }

  if (adc_reading >= adc_deltas[calibration_points - 1])
  {
    return ppm_values[calibration_points - 1]; // Max PPM (20 PPM)
  }

  // Perform interpolation between the two closest points
  for (int i = 0; i < calibration_points - 1; i++)
  {
    if (adc_reading >= adc_deltas[i] && adc_reading < adc_deltas[i + 1])
    {

      // Calculate position within the range (0 to 1)
      float delta_range = adc_deltas[i + 1] - adc_deltas[i];
      float position = (adc_reading - adc_deltas[i]) / delta_range;

      // Apply square root interpolation for smoother curve
      float rms_position = sqrt(position);

      // Interpolate PPM values using RMS position
      float ppm_range = ppm_values[i + 1] - ppm_values[i];
      float ppm = ppm_values[i] + rms_position * ppm_range;

      // Debug output
      Serial.print(F("ADC: "));
      Serial.print(adc_reading);
      Serial.print(F(" | Range: ["));
      Serial.print(adc_deltas[i]);
      Serial.print(F(" - "));
      Serial.print(adc_deltas[i + 1]);
      Serial.print(F("] | Position: "));
      Serial.print(position, 3);
      Serial.print(F(" | RMS Position: "));
      Serial.print(rms_position, 3);
      Serial.print(F(" | PPM: "));
      Serial.println(ppm, 2);

      return ppm;
    }
  }

  // Fallback - should not happen with correct calibration
  return 0.0;
}

void drawBattery()
{
  // TODO: Draw battery indicator at top-left accurately
  int x = 2, y = 2, bw = 18, bh = 8;
  display.drawRect(x, y, bw, bh, SSD1306_WHITE);
  display.fillRect(x + bw, y + 2, 2, bh - 4, SSD1306_WHITE);
  // Show full battery level as placeholder
  display.fillRect(x + 2, y + 2, bw - 4, bh - 4, SSD1306_WHITE);
}

void blowOutAnimation()
{
  lowestADC = 1023;
  highestADC = 0;

  for (int t = 0; t < 50; t++)
  {
    display.clearDisplay();

    drawBattery();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 25);
    display.println(F("Blow"));
    display.setCursor(21, 35);
    display.println(F("Out"));

    int barX = 24;
    int barY = 50;
    int barWidth = 16;
    int barHeight = 50;

    display.drawRoundRect(barX, barY, barWidth, barHeight, 4, SSD1306_WHITE);

    // Calculate fill based on breathing cycle (emptying animation)
    float breathCycle = (float)t / 50.0; // 0 to 1

    // Fill from bottom up and decrease (emptying animation)
    int fillHeight = (int)((barHeight - 4) * (1.0 - breathCycle));
    if (fillHeight > 0)
    {
      display.fillRoundRect(barX + 2, barY + 2 + (barHeight - 4 - fillHeight),
                            barWidth - 4, fillHeight, 2, SSD1306_WHITE);
    }

    int currentADC = analogRead(ACETONE_ADC_PIN);
    if (currentADC < lowestADC)
    {
      lowestADC = currentADC;
    }
    if (currentADC > highestADC)
    {
      highestADC = currentADC;
    }

    Serial.print(F("Current ADC: "));
    Serial.print(currentADC);
    Serial.print(F(" | Lowest ADC: "));
    Serial.print(lowestADC);
    Serial.print(F(" | Highest ADC: "));
    Serial.println(highestADC);

    display.display();
    delay(100); // 100ms delay for smooth animation
  }
}

float getAcetoneLevel()
{
  int adcValue = highestADC - lowestADC;

  if (adcValue < 0)
  {
    adcValue = 0;
  }

  float vb_gas = (adcValue * 5.0) / 1024.0;

  float delta_vout = vb_gas - VB_AIR;
  float delta_vout_mv = delta_vout * 1000.0;

  float ppmValue = adcToPPM(adcValue);

  Serial.print(F("Final ADC Delta: "));
  Serial.print(adcValue);
  Serial.print(F("\tVb(Gas): "));
  Serial.print(vb_gas, 3);
  Serial.print(F("\tΔVOUT(mV): "));
  Serial.print(delta_vout_mv, 1);
  Serial.print(F("\tPPM: "));
  Serial.println(ppmValue, 1);

  return ppmValue;
}

void processingAnimation()
{
  for (int cycle = 0; cycle < 30; cycle++)
  { // 30 cycles for 3 seconds (100ms each)
    display.clearDisplay();

    drawBattery();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(1, 35);
    display.println(F("Processing"));

    // Draw rotating circle animation
    int centerX = 32;
    int centerY = 65;
    int radius = 10;

    // Draw circle outline
    display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);

    // Draw rotating dots
    float startAngle = (cycle * 24) * (PI / 180.0); // 24 degrees per cycle
    int numDots = 8;

    for (int i = 0; i < numDots; i++)
    {
      float angle = startAngle + (i * 2 * PI / numDots);
      int dotX = centerX + (radius - 2) * cos(angle);
      int dotY = centerY + (radius - 2) * sin(angle);

      if (i < (cycle % numDots + 1))
      {
        display.fillCircle(dotX, dotY, 1, SSD1306_WHITE);
      }
    }

    display.display();
    delay(100);
  }
}

String getKetoStatus(float ppmValue)
{
  if (ppmValue < 1.0)
    return F("  None  ");
  else if (ppmValue < 5.0)
    return F("  Light ");
  else if (ppmValue < 10.0)
    return F("Moderate");
  else if (ppmValue < 15.0)
    return F("  High  ");
  else
    return F("Very High");
}

void displayResults()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  float finalPPM = getAcetoneLevel();
  // // float finalPPM = getAcetoneLevel();

  // if (finalPPM < 4) {
  //   // Test display
  //   finalPPM = 5 + (rand() % 2); // rand() % 6 generates a random number from 0 to 5
  // }
  drawBattery();

  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println(F("Acetone"));
  display.setCursor(15, 30);
  display.println(F("Level"));

  display.setTextSize(2);
  display.setCursor(15, 50);
  display.print(finalPPM, 1);

  display.setTextSize(1);
  String ketoStatus = getKetoStatus(finalPPM);
  display.setCursor(10, 75);
  display.println(F("Ketosis:"));
  display.setCursor(5, 90);
  display.println(ketoStatus);

  int barWidth = 40;
  int barHeight = 4;
  int barX = 10;
  int barY = 110;

  display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);

  int fillWidth = 0;
  if (ketoStatus == "  None  ")
    fillWidth = barWidth * 0.1;
  else if (ketoStatus == "  Light ")
    fillWidth = barWidth * 0.4;
  else if (ketoStatus == "Moderate")
    fillWidth = barWidth * 0.7;
  else if (ketoStatus == "  High  ")
    fillWidth = barWidth * 0.9;
  else
    fillWidth = barWidth;

  if (fillWidth > 2)
  {
    display.fillRect(barX + 1, barY + 1, fillWidth - 2, barHeight - 2, SSD1306_WHITE);
  }

  display.display();
}

void welcomeScreen()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  drawBattery();

  display.setCursor(8, 50);
  display.println(F("Welcome"));
  display.setCursor(25, 60);
  display.println(F("to"));
  display.setCursor(17, 70);
  display.println(F("Keto"));
  display.display();
}

void countdown()
{
  for (int i = 15; i >= 1; i--)
  {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    drawBattery();

    display.setTextSize(1);
    display.setCursor(15, 30);
    display.println(F("Let's"));
    display.setCursor(15, 40);
    display.println(F("Start"));

    display.setTextSize(3);
    display.setCursor(15, 70);
    if (i < 10)
    {
      display.print(F("0"));
    }
    display.print(i);
    display.display();
    delay(1000);
  }
}

void loop()
{
  welcomeScreen();

  delay(5000);

  countdown();

  blowOutAnimation();

  processingAnimation();

  displayResults();

  delay(5000);
}