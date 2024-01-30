#pragma once

class Electroniccats_PN7150;
class TwoWire;

struct menu {
    int iUserID;
    int iMenuLen;
    int iaMenu[12];

    void Print()
    {
        Serial.printf("UserID: %d\n", iUserID);
        Serial.printf("MenuLen: %d\n", iMenuLen);
        for( int i = 0; i < iMenuLen; i++ )
            Serial.printf("\t%d\n", iaMenu[i]);
    }

    void Clear()
    {
        iUserID = 0;
        iMenuLen = 0;
        memset(iaMenu, 0, sizeof(int) * 12);
    }
};

class CNFCHandler {
public:
    void setup( TwoWire *wire, int SDA_Pin, int SCL_Pin, int IRQ_Pin, int VEN_Pin );
    void loop();

    Electroniccats_PN7150 GetNFC() { return *m_NFC; }

    bool CheckCard(bool bWait = false);
    void WaitForRemoval();
    void Reset();
    int WriteMenu(menu newMenu);
    int ReadMenu(menu &out);
private:
    TwoWire *m_Wire;
    Electroniccats_PN7150 *m_NFC;
};