#include <Servo.h>

#define STX_NUM 0
#define LENGTH_NUM 1
#define PROTOCOL_NUM 2

#define STX(DATA) (DATA[STX_NUM])
#define LENGTH(DATA) (DATA[LENGTH_NUM])
#define PROTOCOL(DATA) (DATA[PROTOCOL_NUM])
#define ETX_NUM(DATA) (LENGTH(DATA) - 1)
#define ETX(DATA) (DATA[ETX_NUM(DATA)])

#define CHECK_NUM(DATA) (ETX_NUM(DATA) - 1)
#define CHECK(DATA) (DATA[CHECK_NUM(DATA)])
#define LAST(DATA) (DATA[LENGTH(DATA)])

int servoPin = 9;
Servo servo;
int angle = 0; // servo position in degrees

byte protocol_data[256] = { 0, };
byte protocol_send[256] = { 0, };

bool check_checksum(byte* data) {
  int size = LENGTH(data);
  int check = 0;

  Serial.println("AAAEEAEAE");
  for (int i = 0; i < size; i++) {
    check ^= data[i];
  }
  
  if (check == 0) {
    return true;
  } else {
    return false;
  }
}

void make_checksum(byte* data) {
  int size = LENGTH(data);
  int check = 0;

  for (int i = 0; i < size; i++) {
    check ^= data[i];
  }
  
  CHECK(data) = check;
}

void send_nack(int protocol) {
  byte data[32] = { 0, };

  STX(data) = '$';
  LENGTH(data) = 6;
  PROTOCOL(data) = protocol;
  data[3] = 73;
  ETX(data) = '#';
  LAST(data) = 0;
  
  make_checksum(data);
  Serial.write(data, LENGTH(data));
}


void read_data() {
  if (Serial.available() > 0) {
    byte stx = Serial.read();
    if (stx == '$') {
      STX(protocol_data) = '$';

      while (Serial.available() == 0);
      LENGTH(protocol_data) = Serial.read() - '0';

      if (sizeof(protocol_data) < LENGTH(protocol_data)) {
        send_nack(PROTOCOL(protocol_data));
      }

      for (int i = PROTOCOL_NUM; i < LENGTH(protocol_data); i++) {
        while (Serial.available() == 0);
        protocol_data[i] = Serial.read();
      }
      
      // Checksum 검사
      if (!check_checksum(protocol_data)) {
        send_nack(PROTOCOL(protocol_data));
      }
      // ETX 값 검사.
      else if (ETX(protocol_data) != '#') {
        send_nack(PROTOCOL(protocol_data));
      } else {
        if (PROTOCOL(protocol_data) == 'A') {
          servo.write(+100);
          delay(500);
        }else {
          servo.write(-100);
          delay(500);
        }
      }
    }
  }
}

void setup()
{
  randomSeed(analogRead(0));

  Serial.begin(9600);
  Serial.println("@0:setup start");
  servo.attach(servoPin);
  Serial.println(sizeof(int));
  Serial.println("@0:servo motor setup");
  servo.write(-100);
  delay(500);

  Serial.println("@0:setup finished");
}


void loop()
{
  read_data();
}
