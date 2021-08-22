#include "ESP8266.h"

// Wi-Fi SSID
#define SSID        ""//ルーターのSSID
// Wi-Fi PASSWORD
#define PASSWORD    ""//ルーターのパスワード
// サーバーのホスト名
#define HOST_NAME   ""//KAMAKEサーバーのURL
// ポート番号
#define HOST_PORT   80
//　ボタンを消灯に設定
#define buttonON LOW
//ボタンの状態管理
int state;
//センサ距離取得
float Vcc = 5.0;
float dist1;
float dist2;
//経過時間計測
unsigned long time1;
unsigned long time2;
double time3;
//変数の扱い
int maxtime = 15;//未使用時から自動で満→空になる秒数
int usedis = 20;//使用状況検知距離

ESP8266 wifi(Serial1);

#define DIR_NAME      ""  //サーバーのパス

#define SENT_MESSAGE1  "満"                        //送信内容
#define SENT_MESSAGE2  "空"                        //送信内容

#define MAIL_ADDRESS  ""                 //メールアドレス
#define MAIL_SUBJECT  "IoTセミナー"                  //メール件名
#define MAIL_MESSAGE  "メール送信成功"                //メール本文

  // サーバーへ渡す情報
const char logGet[] PROGMEM = "GET "DIR_NAME"/logGet.php HTTP/1.0\r\nHost: kamake.co.jp\r\nUser-Agent: arduino\r\n\r\n";
const char logSent1[] PROGMEM = "GET "DIR_NAME"/logSent.php?message="SENT_MESSAGE1" HTTP/1.0\r\nHost: kamake.co.jp\r\nUser-Agent: arduino\r\n\r\n";
const char logSent2[] PROGMEM = "GET "DIR_NAME"/logSent.php?message="SENT_MESSAGE2" HTTP/1.0\r\nHost: kamake.co.jp\r\nUser-Agent: arduino\r\n\r\n";
const char mailSent[] PROGMEM = "GET "DIR_NAME"/mailSent.php?address=";
const char *const send_table[] PROGMEM = {
  logSent1, logSent2, mailSent, logGet 
};

/**
 * 初期設定
 */
void setup(void)
{
  Serial.begin(9600);
  state = 0;
  pinMode(2, INPUT_PULLUP);//スイッチ用pin
  pinMode(3, OUTPUT);//LED用pin
  pinMode(13, OUTPUT);//led用pin

  while (1) {
    Serial.print("restaring esp8266...");
    if (wifi.restart()) {
      Serial.print("ok\r\n");
      break;
    }
    else {
      Serial.print("not ok...\r\n");
      Serial.print("Trying to kick...");
      while (1) {
        if (wifi.kick()) {
          Serial.print("ok\r\n");
          break;
        }
        else {
          Serial.print("not ok... Wait 5 sec and retry...\r\n");
          delay(5000);
        }
      }
    }
  }
  
  Serial.print("setup begin\r\n");

  Serial.print("FW Version:");
  Serial.println(wifi.getVersion().c_str());

  if (wifi.setOprToStationSoftAP()) {
    Serial.print("to station + softap ok\r\n");
  } else {
    Serial.print("to station + softap err\r\n");
  }

  if (wifi.joinAP(SSID, PASSWORD)) {
    Serial.print("Join AP success\r\n");
    Serial.print("IP:");
    Serial.println( wifi.getLocalIP().c_str());
  } else {
    Serial.print("Join AP failure\r\n");
  }

  if (wifi.disableMUX()) {
    Serial.print("single ok\r\n");
  } else {
    Serial.print("single err\r\n");
  }
 
  Serial.print("setup end\r\n");
}

//
// ループ処理
//
void loop(void)
{
  uint8_t buffer[340] = {0};
  char sendStr[280];
  static int j = 0;
  int flag = 0;//0:メールアドレスを受け取っていない、1:メールアドレスを受け取っている

  char mailStr[128] = "&subject="MAIL_SUBJECT"&message="MAIL_MESSAGE" HTTP/1.0\r\nHost: kamake.co.jp\r\nUser-Agent: arduino\r\n\r\n";
  char mailad[100];
  //メールアドレスを受け取る
  if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
    Serial.print("create tcp ok\r\n");
  } else {
    Serial.print("create tcp err\r\n");
  }
  strcpy_P(sendStr, (char *)pgm_read_word(&(send_table[3])));
  wifi.send((const uint8_t*)sendStr, strlen(sendStr));
  //サーバからの文字列を入れるための変数
  String resultCode = "";
  // 取得した文字列の長さ
  uint32_t len = wifi.recv(buffer, sizeof(buffer), 10000);
  // 取得した文字数が0でなければ
  if (len > 0) {
    flag = 1;
    for(uint32_t i = 0; i < len; i++) {
      resultCode += (char)buffer[i];
    }
    // lastIndexOfでresultCodeの最後から改行を探す
    int lastLF = resultCode.lastIndexOf('\n');
    // resultCodeの長さを求める
    int resultCodeLength = resultCode.length();
    // substringで改行コードの次の文字から最後までを求める
    String resultstring = resultCode.substring(lastLF+1, resultCodeLength);
    // 前後のスペースを取り除く
    resultstring.trim();
    Serial.print("mailadress = ");
    Serial.println(resultstring);
    int adress_len = resultstring.length() + 1;
    resultstring.toCharArray(mailad, adress_len);
    Serial.println(mailad);
  }
  //使用状況チェック
  //ボタンが押されている
  if(digitalRead(2) == buttonON){//使用開始
    if(state == 0){
      state = 1;
      digitalWrite(3,HIGH);
    }
    else if(state == 2){//使用終了
      state = 3;
      digitalWrite(3,LOW);
      //メール送信
      if(flag == 1){
        if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
          Serial.print("create tcp ok\r\n");
        } else {
          Serial.print("create tcp err\r\n");
        }
        strcpy_P(sendStr, (char *)pgm_read_word(&(send_table[2])));
        strcat(sendStr, mailad);
        strcat(sendStr, mailStr);
        Serial.println(sendStr);
        wifi.send((const uint8_t*)sendStr, strlen(sendStr));
        Serial.println("send mail");
      }
    }
  }// end if(buttonON)
  else{//ボタンが押されていない
    if(state == 1) {
      state = 2;
      time1 = millis();
      Serial.println("使用開始");
    }
    if(state == 3) state = 0;
    if(state == 0){
      //logSent(”空”)
      if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
        Serial.print("create tcp ok\r\n");
      } else {
        Serial.print("create tcp err\r\n");
      }
      strcpy_P(sendStr, (char *)pgm_read_word(&(send_table[1])));
      wifi.send((const uint8_t*)sendStr, strlen(sendStr));
      Serial.println("空");
    }
    else if(state == 2){
      //logSent("満")
      if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
        Serial.print("create tcp ok\r\n");
      } else {
        Serial.print("create tcp err\r\n");
      }
      strcpy_P(sendStr, (char *)pgm_read_word(&(send_table[0])));
      wifi.send((const uint8_t*)sendStr, strlen(sendStr));
      Serial.println("満");
      //センサ距離取得
      dist1 = Vcc*analogRead(A0)/1023;
      dist2 = 26.549*pow(dist1,-1.2091);//こうすると近似式になる
      Serial.print(dist2);
      Serial.println("cm");
      if(dist2<usedis){//使用しているということ
        time1 = millis();
        Serial.println("使用中");
      }
      time2 = millis() - time1;
      if((time2/1000) > maxtime){//秒単位
        Serial.print("最終使用から");
        Serial.print(maxsec);
        Serial.println("秒経過しました");
        time3 = (double)time2/1000;
        Serial.print(time3);
        Serial.println("秒"); 
        state = 0;
        //logSent("空")
        if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
           Serial.print("create tcp ok\r\n");
        } else {
           Serial.print("create tcp err\r\n");
        }
        strcpy_P(sendStr, (char *)pgm_read_word(&(send_table[1])));
        wifi.send((const uint8_t*)sendStr, strlen(sendStr));
        Serial.println("空");
        digitalWrite(3,LOW);//LED OFF
        //計測時間の初期化
        time3 = 0;
        //メール送信
          if(flag == 1){
            if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
              Serial.print("create tcp ok\r\n");
           } else {
              Serial.print("create tcp err\r\n");
           }
           strcpy_P(sendStr, (char *)pgm_read_word(&(send_table[2])));
           strcat(sendStr, mailad);
           strcat(sendStr, mailStr);
           Serial.println(sendStr);
           wifi.send((const uint8_t*)sendStr, strlen(sendStr));
           Serial.println("send mail");
        }
      }//end if(time2 > maxtime)
    }//end else if(state = 2)
  }// end else
  
  // 500ミリ秒待つ
  delay(500);
}
