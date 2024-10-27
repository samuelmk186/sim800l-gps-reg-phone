#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <EEPROM.h>

const int totalPhoneNo = 5;
String phoneNo[totalPhoneNo] = {"", "", "", "", ""};
int offsetPhone[totalPhoneNo] = {0, 13, 26, 39, 52};
String tempPhone = "";

#define rxGPS 5
#define txGPS 4
SoftwareSerial neogps(rxGPS, txGPS);
TinyGPSPlus gps;

#define rxGSM 0
#define txGSM 2
SoftwareSerial sim800(rxGSM, txGSM);

String smsStatus;
String senderNumber;
String receivedDate;
String msg;

boolean DEBUG_MODE = 1;

void setup() {
    Serial.begin(115200);
    Serial.println("NodeMCU USB serial initialize");
    sim800.begin(9600);
    Serial.println("SIM800L serial initialize");
    neogps.begin(9600);
    Serial.println("NEO6M serial initialize");

    sim800.listen();
    neogps.listen();
    EEPROM.begin(512);

    Serial.println("List of Registered Phone Numbers");
    for (int i = 0; i < totalPhoneNo; i++) {
        phoneNo[i] = readFromEEPROM(offsetPhone[i]);
        if (phoneNo[i].length() != 13) {
            phoneNo[i] = "";
            Serial.println(String(i + 1) + ": empty");
        } else {
            Serial.println(String(i + 1) + ": " + phoneNo[i]);
        }
    }

    smsStatus = "";
    senderNumber = "";
    receivedDate = "";
    msg = "";

    delay(9000);
    sim800.println("AT+CMGF=1");
    delay(1000);
    sim800.println("AT+CLIP=1");
    delay(500);
}

void loop() {
    while (sim800.available()) {
        parseData(sim800.readString());
    }
    while (Serial.available()) {
        sim800.println(Serial.readString());
    }
}

void parseData(String buff) {
    Serial.println(buff);

    if (buff.indexOf("RING") > -1) {
        boolean flag = 0;
        String callerID = "";

        if (buff.indexOf("+CLIP:")) {
            unsigned int index, index1;
            index = buff.indexOf("\"");
            index1 = buff.indexOf("\"", index + 1);
            callerID = buff.substring(index + 2, index1);
            callerID.trim();
            if (callerID.length() == 13) {
                flag = comparePhone(callerID);
            } else if (callerID.length() == 10) {
                flag = compareWithoutCountryCode(callerID);
                callerID = "0" + callerID;
            }
        }

        if (flag == 1) {
            sim800.println("ATH");
            delay(1000);
            sendLocation(callerID);
        } else {
            sim800.println("ATH");
            debugPrint("The phone number is not registered.");
        }
        return;
    }

    unsigned int len, index;
    index = buff.indexOf("\r");
    buff.remove(0, index + 2);
    buff.trim();

    if (buff != "OK") {
        index = buff.indexOf(":");
        String cmd = buff.substring(0, index);
        cmd.trim();
        buff.remove(0, index + 2);

        if (cmd == "+CMTI") {
            index = buff.indexOf(",");
            String temp = buff.substring(index + 1, buff.length());
            temp = "AT+CMGR=" + temp + "\r";
            sim800.println(temp);
        } else if (cmd == "+CMGR") {
            extractSms(buff);
            if (msg.equals("r") && phoneNo[0].length() == 13) {
                writeToEEPROM(offsetPhone[0], senderNumber);
                phoneNo[0] = senderNumber;
                String text = "Number is Registered: ";
                text = text + senderNumber;
                debugPrint(text);
                Reply("Number is Registered", senderNumber);
            }
            if (comparePhone(senderNumber)) {
                doAction(senderNumber);
            }
        }
    }
}

void doAction(String phoneNumber) {
    if (msg == "send location") {
        sendLocation(phoneNumber);
    }
    if (msg == "r2=") {
        Serial.println(offsetPhone[1]);
        writeToEEPROM(offsetPhone[1], tempPhone);
        phoneNo[1] = tempPhone;
        String text = "Phone2 is Registered: ";
        text = text + tempPhone;
        debugPrint(text);
        Reply(text, phoneNumber);
    } else if (msg == "r3=") {
        writeToEEPROM(offsetPhone[2], tempPhone);
        phoneNo[2] = tempPhone;
        String text = "Phone3 is Registered: ";
        text = text + tempPhone;
        Reply(text, phoneNumber);
    } else if (msg == "r4=") {
        writeToEEPROM(offsetPhone[3], tempPhone);
        phoneNo[3] = tempPhone;
        String text = "Phone4 is Registered: ";
        text = text + tempPhone;
        Reply(text, phoneNumber);
    } else if (msg == "r5=") {
        writeToEEPROM(offsetPhone[4], tempPhone);
        phoneNo[4] = tempPhone;
        String text = "Phone5 is Registered: ";
        text = text + tempPhone;
        Reply(text, phoneNumber);
    } else if (msg == "list") {
        String text = "";
        if (phoneNo[0]) text = text + phoneNo[0] + "\r\n";
        if (phoneNo[1]) text = text + phoneNo[1] + "\r\n";
        if (phoneNo[2]) text = text + phoneNo[2] + "\r\n";
        if (phoneNo[3]) text = text + phoneNo[3] + "\r\n";
        if (phoneNo[4]) text = text + phoneNo[4] + "\r\n";
        debugPrint("List of Registered Phone Numbers: \r\n" + text);
        Reply(text, phoneNumber);
    } else if (msg == "del=1") {
        writeToEEPROM(offsetPhone[0], "");
        phoneNo[0] = "";
        Reply("Phone1 is deleted.", phoneNumber);
    } else if (msg == "del=2") {
        writeToEEPROM(offsetPhone[1], "");
        phoneNo[1] = "";
        debugPrint("Phone2 is deleted.");
        Reply("Phone2 is deleted.", phoneNumber);
    } else if (msg == "del=3") {
        writeToEEPROM(offsetPhone[2], "");
        phoneNo[2] = "";
        debugPrint("Phone3 is deleted.");
        Reply("Phone3 is deleted.", phoneNumber);
    } else if (msg == "del=4") {
        writeToEEPROM(offsetPhone[3], "");
        phoneNo[3] = "";
        debugPrint("Phone4 is deleted.");
        Reply("Phone4 is deleted.", phoneNumber);
    } else if (msg == "del=5") {
        writeToEEPROM(offsetPhone[4], "");
        phoneNo[4] = "";
        debugPrint("Phone5 is deleted.");
        Reply("Phone5 is deleted.", phoneNumber);
    }
    if (msg == "del=all") {
        writeToEEPROM(offsetPhone[0], "");
        writeToEEPROM(offsetPhone[1], "");
        writeToEEPROM(offsetPhone[2], "");
        writeToEEPROM(offsetPhone[3], "");
        writeToEEPROM(offsetPhone[4], "");
        phoneNo[0] = "";
        phoneNo[1] = "";
        phoneNo[2] = "";
        phoneNo[3] = "";
        phoneNo[4] = "";
        offsetPhone[0] = NULL;
        offsetPhone[1] = NULL;
        offsetPhone[2] = NULL;
        offsetPhone[3] = NULL;
        offsetPhone[4] = NULL;
        debugPrint("All phone numbers are deleted.");
        Reply("All phone numbers are deleted.", phoneNumber);
    }

    smsStatus = "";
    senderNumber = "";
    receivedDate = "";
    msg = "";
    tempPhone = "";
}

void extractSms(String buff) {
    unsigned int index;
    index = buff.indexOf(",");
    smsStatus = buff.substring(1, index - 1);
    buff.remove(0, index + 2);

    senderNumber = buff.substring(0, 13);
    buff.remove(0, 19);

    receivedDate = buff.substring(0, 20);
    buff.remove(0, buff.indexOf("\r"));
    buff.trim();

    index = buff.indexOf("\n\r");
    buff = buff.substring(0, index);
    buff.trim();
    msg = buff;
    buff = "";
    msg.toLowerCase();

    String tempcmd = msg.substring(0, 3);
    if (tempcmd.equals("r1=") || tempcmd.equals("r2=") ||
        tempcmd.equals("r3=") || tempcmd.equals("r4=") ||
        tempcmd.equals("r5=")) {
        tempPhone = msg.substring(3, 16);
        msg = tempcmd;
    }
}

void Reply(String text, String Phone) {
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\"" + Phone + "\"\r");
    delay(2000);
    sim800.print(text);
    sim800.write(0x1A);
    delay(1000);
    sim800.println("AT+CMGD=1,4");
    delay(500);
}

void writeToEEPROM(int addrOffset, String data) {
    byte len = data.length();
    EEPROM.write(addrOffset, len);
    for (int i = 0; i < len; i++) {
        EEPROM.write(addrOffset + 1 + i, data[i]);
    }
    EEPROM.commit();
}

String readFromEEPROM(int addrOffset) {
    int newStrLen = EEPROM.read(addrOffset);
    char data[newStrLen + 1];
    for (int i = 0; i < newStrLen; i++) {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }
    data[newStrLen] = '\0';
    return String(data);
}

void sendLocation(String phoneNumber) {
    debugPrint("Sending Location to " + phoneNumber);
    String text = "";
    for (unsigned long start = millis(); millis() - start < 1000;) {
        while (neogps.available()) {
            gps.encode(neogps.read());
        }
    }
    if (gps.location.isValid()) {
        text = "https://maps.google.com/maps?q=loc:" +
               String(gps.location.lat(), 6) + "," +
               String(gps.location.lng(), 6);
        Reply(text, phoneNumber);
    } else {
        debugPrint("No Location Found");
        Reply("No Location Found", phoneNumber);
    }
}

boolean comparePhone(String PhoneNumber) {
    for (int i = 0; i < totalPhoneNo; i++) {
        if (phoneNo[i] == PhoneNumber) return true;
    }
    return false;
}

boolean compareWithoutCountryCode(String PhoneNumber) {
    for (int i = 0; i < totalPhoneNo; i++) {
        if (phoneNo[i].substring(3, phoneNo[i].length()) == PhoneNumber) return true;
    }
    return false;
}

void debugPrint(String text) {
    if (DEBUG_MODE == 1) Serial.println(text);
}
