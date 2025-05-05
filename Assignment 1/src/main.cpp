#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  while (!Serial){
    ;
  }
  delay(1000);
}

void loop() {
  Serial.println("Hello, world!");
  Serial.println("Goodbye!");
  delay(2000);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}