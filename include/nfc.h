#pragma once

class Electroniccats_PN7150;
class TwoWire;

class CNFCHandler {
public:
    void setup( TwoWire *wire, int SDA_Pin, int SCL_Pin, int IRQ_Pin, int VEN_Pin );
    void loop();

    Electroniccats_PN7150 GetNFC() { return *m_NFC; }

    bool CheckCard(bool bWait = false);
    void WaitForRemoval();
    void Reset();
private:
    TwoWire *m_Wire;
    Electroniccats_PN7150 *m_NFC;
};