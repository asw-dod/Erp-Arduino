#include <Servo.h>

// 테스트와 디버깅을 하기 위한 정의
// ETX, Checksum 검사 유무
// DEBUG_MODE가 정의가 되어 있다면 검사를 안함.
#define DEBUG_MODE 

// $5AC# 처럼 "5"가 ASCII 5로 인식할지, 바이너리 숫자 5로 인식할지 결정함.
// "5"는 0x35 임. TEST_MODE가 정의가 되어있다면 '5'를 5로 인식함.
// 정의가 되어있지 않다면 0x05를 5로 인식함.
#define TEST_MODE

// STX 인덱스 번호
#define STX_NUM 0

// 문자열 길이 인덱스 번호
#define LENGTH_NUM 1

// 프로토콜 인덱스 번호
#define PROTOCOL_NUM 2

// DATA 변수에서 STX의 주소를 반환 함.
#define STX(DATA) (DATA[STX_NUM])

// DATA 변수에서 데이터길이의 주소를 반환 함.
#define LENGTH(DATA) (DATA[LENGTH_NUM])

// DATA 변수에서 프로토콜의 주소를 반환 함.
#define PROTOCOL(DATA) (DATA[PROTOCOL_NUM])

// DATA 변수에서 ETX의 인덱스 번호
#define ETX_NUM(DATA) (LENGTH(DATA) - 1)

// DATA 변수에서 ETX의 주소를 반환 함.
#define ETX(DATA) (DATA[ETX_NUM(DATA)])

// DATA 변수에서 CHECK SUM의 인덱스 번호
#define CHECK_NUM(DATA) (ETX_NUM(DATA) - 1)

// DATA 변수에서 CHECK SUM의 주소를 반환 함.
#define CHECK(DATA) (DATA[CHECK_NUM(DATA)])

// DATA 변수에서 데이터의 (ETX + 1)의 주소를 반환 함.
#define LAST(DATA) (DATA[LENGTH(DATA)])

// A 프로토콜의 데이터 정의 구조체
struct recv_device_health {
  byte stx;
  byte length;
  byte protocol;
  byte data_connect_check: 1;
  byte check_sum;
  byte etx;
};

// B 프로토콜의 데이터 정의 구조체
struct recv_device_lock {
  byte stx;
  byte length;
  byte protocol;
  // 현재 잠금 상태인지 확인 하는 변수
  byte data_lock: 1;
  byte check_sum;
  byte etx;
};

// A 프로토콜의 응답 데이터 정의 구조체
struct ack_device_health {
  byte stx;
  byte length;
  byte protocol;
  // 아두이노 장비의 Health
  byte data_device_health: 1;
  // 아두이노 모터의 Health
  byte data_motor_health: 1;
  // 현재 잠금 상태인지 확인 하는 변수
  byte data_lock: 1;

  byte check_sum;
  byte etx;
};

// B 프로토콜의 응답 데이터 정의 구조체
struct ack_device_lock {
  byte stx;
  byte length;
  byte protocol;
  byte data_lock;
  byte check_sum;
  byte etx;
};

// 서보 핀 9번 연결할 예정이므로 9로 정의함
int servoPin = 9;

// 서보 변수 정의
Servo servo;

// Recv 데이터 바이트로 정의함.
byte protocol_data[256] = { 0, };

// A 프로토콜 데이터를 처리 할 함수
void device_health(byte* data) {
  // Byte[] 데이터를 구조체에 맵핑함.
  struct recv_device_health recv = *(struct recv_device_health*)(data);
  
  // 값 디버깅 출력.
  Serial.println("@---- RECEIVE ----");
  Serial.println(recv.stx);
  Serial.println(recv.length);
  Serial.println(recv.protocol);
  Serial.println(recv.data_connect_check);
  Serial.println(recv.check_sum);
  Serial.println(recv.etx);
  Serial.println("---- RECEIVE FINISHED ----%");

  // 응답 코드 생성
  struct ack_device_health ack;
  ack.stx = '$';
  ack.length = 6;
  ack.protocol = recv.protocol;
  ack.data_device_health = 1;

  // 8번 
  // digitalRead(8);
  // digitalWrite(8, 255);

  // 모터가 항상 살아있다는 전제로 함.
  ack.data_motor_health = 1;
  // 서보 모터의 값이 양수면 잠금이 되어있다는 의미로 인식함.
  ack.data_lock = servo.read() > 0 ? true : false;
  // 체크섬 값 0으로 대입
  ack.check_sum = 0;
  // ETX 값 
  ack.etx = '#';

  byte* ack_data = (byte*)(&ack);
  // Checksum 계산
  make_checksum(ack_data);
  // 결과를 시리얼로 출력함.
  Serial.write(ack_data, LENGTH(ack_data));
}

// B 프로토콜 응답 함수
void device_lock(byte* data) {
  // 맵핑
  struct recv_device_lock recv = *(struct recv_device_lock*)(data);
  // 디버깅 출력
  Serial.println("@---- RECEIVE ----");
  Serial.println(recv.stx);
  Serial.println(recv.length);
  Serial.println(recv.protocol);
  Serial.println(recv.data_lock);
  Serial.println(recv.check_sum);
  Serial.println(recv.etx);
  Serial.println("---- RECEIVE FINISHED ----%");
  // False면 -100으로 잠금해제.
  // True면 100으로 잠금
  if (recv.data_lock == 0) {
    servo.write(-100);
  } else {
    servo.write(100);
  }

  // 출력
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

// Checksum이 정상적으로 계산이 되는지 체크함.
bool check_checksum(byte* data) {
  int size = LENGTH(data);
  int check = 0;

  for (int i = 0; i < size; i++) {
    check ^= data[i];
  }
  // 모든 값을 XOR 했더니 값이 0인지 확인함.
  // 모든 값을 XOR 하면 당연히 0이 나와야겠죠!
  // 안나온다면 값이 다르단 의미!
  if (check == 0) {
    return true;
  } else {
    return false;
  }
}

// 체크섬을 계산함.
void make_checksum(byte* data) {
  // LENGTH로 데이터 길이를 구한 뒤
  // check로 0번째 인덱스부터 LENGTH - 1 인덱스 까지 모두 XOR 연산 함.
  // CHECK 데이터는 0으로 인식함.
  int size = LENGTH(data);
  int check = 0;

  for (int i = 0; i < size; i++) {
    check ^= data[i];
  }
  // CHECK 데이터 대입.
  CHECK(data) = check;
}

// 정상적으로 인식을 못했다면 NACK로 응답함.
void send_nack(int protocol) {
  // NACK 응답은 PROTOCOL 
  byte data[32] = { 0, };

  // NACK는 항상 동일 하므로 저렇게 정의 함.
  STX(data) = '$';
  LENGTH(data) = 7;
  PROTOCOL(data) = 0x41;
  data[3] = protocol;
  data[4] = 73;
  CHECK(data) = 0;
  ETX(data) = '#';
  LAST(data) = '\0';

  make_checksum(data);
  Serial.write(data, LENGTH(data));
}

// 데이터를 읽고 파싱함.
void read_data() {
  if (Serial.available() > 0) {
    byte stx = Serial.read();
    Serial.print("@첫번째 데이터 인식 코드 : ");
    Serial.print(stx);
    Serial.println("%");
    
    if (stx == '$') {
      Serial.println("@데이터 STX 인식%");
      STX(protocol_data) = '$';
      
      while (Serial.available() == 0);
#ifdef TEST_MODE
      LENGTH(protocol_data) = Serial.read() - '0';
#else
      LENGTH(protocol_data) = Serial.read();
#endif
      Serial.print("@");
      Serial.print(LENGTH(protocol_data));
      Serial.println(": 사이즈 인식%");

      if (sizeof(protocol_data) < LENGTH(protocol_data)) {
        Serial.println("@데이터의 크기가 비 정상적으로 큼%");
        send_nack(PROTOCOL(protocol_data));
      }

      for (int i = PROTOCOL_NUM; i < LENGTH(protocol_data); i++) {
        while (Serial.available() == 0);
        protocol_data[i] = Serial.read();
      }

      // Checksum 검사
#ifndef DEBUG_MODE
      Serial.println("@Serial Checksum 계산 시작%");
      Serial.println("@Serial ETX 값이 옳바른지 검사%");
      if (!check_checksum(protocol_data)) {
        Serial.println("@Serial Checksum 데이터 무결성 검사 실패%");
        send_nack(PROTOCOL(protocol_data));
      }
      // ETX 값 검사.
      else if (ETX(protocol_data) != '#') {
        Serial.println("@Serial ETX 값이 옳바른지 검사 실패%");
        send_nack(PROTOCOL(protocol_data));
      } else {
#else
      Serial.println("@데이터 무결성 검사 생략 모드%");
#endif
        switch (PROTOCOL(protocol_data)) {
          case 'A':
            Serial.println("@A 프로토콜%");
            device_health(protocol_data);
            break;
          case 'B':
            Serial.println("@B 프로토콜%");
            device_lock(protocol_data);
            break;
          default:
            Serial.println("@알수없는 프로토콜 결과가 나%");
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
