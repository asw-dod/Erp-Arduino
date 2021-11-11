#include <Servo.h>

#define DEBUG_MODE
#define TEST_MODE

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

struct recv_device_health {
  byte stx;
  byte length;
  byte protocol;
  byte data_connect_check: 1;
  byte check_sum;
  byte etx;
};

struct recv_device_lock {
  byte stx;
  byte length;
  byte protocol;
  byte data_lock: 1;
  byte check_sum;
  byte etx;
};

struct ack_device_health {
  byte stx;
  byte length;
  byte protocol;

  byte data_device_health: 1;
  byte data_motor_health: 1;
  byte data_lock: 1;

  byte check_sum;
  byte etx;
};

struct ack_device_lock {
  byte stx;
  byte length;
  byte protocol;
  byte data_lock;
  byte check_sum;
  byte etx;
};

int servoPin = 9;
Servo servo;
int angle = 0; // servo position in degrees

byte protocol_data[256] = { 0, };
byte protocol_send[256] = { 0, };

void device_health(byte* data) {
  struct recv_device_health recv = *(struct recv_device_health*)(data);
  
  Serial.println("@---- RECEIVE ----");
  Serial.println(recv.stx);
  Serial.println(recv.length);
  Serial.println(recv.protocol);
  Serial.println(recv.data_connect_check);
  Serial.println(recv.check_sum);
  Serial.println(recv.etx);
  Serial.println("---- RECEIVE FINISHED ----%");

  struct ack_device_health ack;
  ack.stx = '$';
  ack.length = 6;
  ack.protocol = recv.protocol;
  ack.data_device_health = 1;
  digitalWrite(8, 0);
  digitalRead(8);
  digitalWrite(8, 255);
  
  ack.data_motor_health = digitalRead(8);
  ack.data_lock = servo.read() > 0 ? true : false;
  ack.check_sum = 0;
  ack.etx = '#';

  byte* ack_data = (byte*)(&ack);
  make_checksum(ack_data);
  Serial.write(ack_data, LENGTH(ack_data));
}

void device_lock(byte* data) {
  struct recv_device_lock recv = *(struct recv_device_lock*)(data);
  
  Serial.println("@---- RECEIVE ----");
  Serial.println(recv.stx);
  Serial.println(recv.length);
  Serial.println(recv.protocol);
  Serial.println(recv.data_lock);
  Serial.println(recv.check_sum);
  Serial.println(recv.etx);
  Serial.println("---- RECEIVE FINISHED ----%");
  
  if (recv.data_lock == 0) {
    servo.write(-100);
  }else {
    servo.write(100);
  }

  struct ack_device_lock ack;
  ack.stx = '$';
  ack.length = 6;
  ack.protocol = recv.protocol;
  ack.data_lock = servo.read() > 0 ? true : false;
  ack.check_sum = 0;
  ack.etx = '#';
  byte* ack_data = (byte*)(&ack);

  make_checksum(ack_data);
  Serial.write(ack_data, LENGTH(ack_data));
}

bool check_checksum(byte* data) {
  int size = LENGTH(data);
  int check = 0;

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
  LENGTH(data) = 7;
  PROTOCOL(data) = 0x41;
  data[3] = protocol;
  data[4] = 73;
  ETX(data) = '#';
  LAST(data) = '\0';

  make_checksum(data);
  Serial.write(data, LENGTH(data));
}


void read_data() {
  if (Serial.available() > 0) {
    byte stx = Serial.read();
    if (stx == '$') {
      STX(protocol_data) = '$';

      while (Serial.available() == 0);
#ifdef TEST_MODE
      LENGTH(protocol_data) = Serial.read() - '0';
#else
      LENGTH(protocol_data) = Serial.read();
#endif
      if (sizeof(protocol_data) < LENGTH(protocol_data)) {
        send_nack(PROTOCOL(protocol_data));
      }

      for (int i = PROTOCOL_NUM; i < LENGTH(protocol_data); i++) {
        while (Serial.available() == 0);
        protocol_data[i] = Serial.read();
      }

      // Checksum 검사
#ifndef DEBUG_MODE
      if (!check_checksum(protocol_data)) {
        send_nack(PROTOCOL(protocol_data));
      }
      // ETX 값 검사.
      else if (ETX(protocol_data) != '#') {
        send_nack(PROTOCOL(protocol_data));
      } else {
#endif
        switch (PROTOCOL(protocol_data)) {
          case 'A':
            device_health(protocol_data);
            break;
          case 'B':
            device_lock(protocol_data);
            break;
          default:
            send_nack(PROTOCOL(protocol_data));
            break;
        }
#ifndef DEBUG_MODE
      }
#endif
    }
  }
}

void setup()
{

  Serial.begin(9600);
  Serial.println("@0:setup start");
  servo.attach(servoPin);
  Serial.println(sizeof(int));
  Serial.println("0:servo motor setup");
  Serial.println("0:setup finished%");

  servo.write(-100);
}


void loop()
{
  read_data();
}
