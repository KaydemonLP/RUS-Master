/**
 * Example to read NDEF messages
 * Authors:
 *        Salvador Mendoza - @Netxing - salmg.net
 *        Francisco Torres - Electronic Cats - electroniccats.com
 *
 *  August 2023
 *
 * This code is beerware; if you see me (or any other collaborator
 * member) at the local, and you've found our code helpful,
 * please buy us a round!
 * Distributed as-is; no warranty is given.
 */

#include "Electroniccats_PN7150.h"
#include "nfc.h"

#define PN7150_ADDR (0x28)

#define BLK_NB_MFC 4                                // Block tat wants to be read
#define KEY_MFC 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Default Mifare Classic key

#define BLK_NB_ISO14443_3A (5)  // Block to be read it


void PrintBuf(const byte* data, const uint32_t numBytes) {  // Print hex data buffer in format
  uint32_t szPos;
  for (szPos = 0; szPos < numBytes; szPos++) {
    Serial.print(F("0x"));
    // Append leading 0 for small values
    if (data[szPos] <= 0xF)
      Serial.print(F("0"));
    Serial.print(data[szPos] & 0xff, HEX);
    if ((numBytes > 1) && (szPos != numBytes - 1))
      Serial.print(F(" "));
  }
  Serial.println();
}

void PCD_ISO14443_3A_scenario( Electroniccats_PN7150 *nfc) {
  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  /* Read block */
  unsigned char ReadBlock[] = {0x30, BLK_NB_ISO14443_3A};

  status = nfc->readerTagCmd(ReadBlock, sizeof(ReadBlock), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x00)) {
    Serial.print("Error reading block: ");
    Serial.print(ReadBlock[1], HEX);
    Serial.print(" with error: ");
    Serial.print(Resp[RespSize - 1], HEX);
    return;
  }
  Serial.print("------------------------Block ");
  Serial.print(BLK_NB_ISO14443_3A, HEX);
  Serial.println("-------------------------");
  PrintBuf(Resp, 4);
}

#define I2C_Freq 100000

void CNFCHandler::setup( TwoWire *wire, int SDA_Pin, int SCL_Pin, int IRQ_Pin, int VEN_Pin ) 
{
  m_Wire = wire;
  if(!m_Wire->begin(SDA_Pin, SCL_Pin, I2C_Freq)) //starting I2C Wire
  {
    Serial.println("I2C Wire1 Error. Going idle.");
    while(true)
      delay(1);
  }

  m_NFC = new Electroniccats_PN7150(IRQ_Pin, VEN_Pin, PN7150_ADDR, m_Wire);  // creates a global NFC device interface object, attached to pins 7 (IRQ) and 8 (VEN) and using the default I2C address 0x28

  Serial.println("Read ISO14443-3A(T2T) data block 5 with PN7150");

  Serial.println("Initializing...");
  if (m_NFC->connectNCI()) {  // Wake up the board
    Serial.println("Error while setting up the mode, check connections!");
    while (1)
      ;
  }

  if (m_NFC->configureSettings()) {
    Serial.println("The Configure Settings failed!");
    while (1)
      ;
  }

  if (m_NFC->configMode()) {  // Set up the configuration mode
    Serial.println("The Configure Mode failed!!");
    while (1)
      ;
  }
  m_NFC->startDiscovery();  // NCI Discovery mode
  Serial.println("Waiting for an ISO14443-3A Card...");
}

void CNFCHandler::loop()
{
  if( m_NFC->isTagDetected() )
    Serial.println("Tag detected");
  else
    Serial.println("Tag not detected");

  if(m_NFC->isTagDetected())
  {
    switch (m_NFC->remoteDevice.getProtocol()) 
    {
      case PROT_T2T:
        Serial.println(" - Found ISO14443-3A(T2T) card");
        switch (m_NFC->remoteDevice.getModeTech()) {  // Indetify card technology
          case (MODE_POLL | TECH_PASSIVE_NFCA):
            char tmp[16];
            Serial.print("\tSENS_RES = ");
            sprintf(tmp, "0x%.2X", m_NFC->remoteDevice.getSensRes()[0]);
            Serial.print(tmp);
            Serial.print(" ");
            sprintf(tmp, "0x%.2X", m_NFC->remoteDevice.getSensRes()[1]);
            Serial.print(tmp);
            Serial.println(" ");

            Serial.print("\tNFCID = ");
            PrintBuf(m_NFC->remoteDevice.getNFCID(), m_NFC->remoteDevice.getNFCIDLen());

            if (m_NFC->remoteDevice.getSelResLen() != 0) {
              Serial.print("\tSEL_RES = ");
              sprintf(tmp, "0x%.2X", m_NFC->remoteDevice.getSelRes()[0]);
              Serial.print(tmp);
              Serial.println(" ");
            }
            break;
        }
        PCD_ISO14443_3A_scenario(m_NFC);
        break;

      default:
        Serial.print(" - Found a card, but it is not ISO14443-3A(T2T)!: ");
        Serial.println(m_NFC->remoteDevice.getModeTech());
        Serial.println(MODE_POLL | TECH_PASSIVE_NFCA);
        break;
    }

    //* It can detect multiple cards at the same time if they use the same protocol
    if (m_NFC->remoteDevice.hasMoreTags()) {
      m_NFC->activateNextTagDiscovery();
    }

    Serial.println("Remove the Card");
    m_NFC->waitForTagRemoval();
    Serial.println("CARD REMOVED!");
  }

  Serial.println("Restarting...");
  m_NFC->reset();
  Serial.println("Waiting for an ISO14443-3A Card...");
  delay(500);
}