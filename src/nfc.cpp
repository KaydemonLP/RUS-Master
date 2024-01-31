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
#define PN7150_ADDR (0x28)

#define BLK_NB_MFC 4                                // Block that wants to be read
#define KEY_MFC 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Default Mifare Classic key

#define AUTH_CODE 'o', 'p'

// Data to be written in the Mifare Classic block
// Structure AUTHCODE_1, AUTHCODE_2, USERID, MENULEN, MENU_ITEM
#define DATA_WRITE_MFC AUTH_CODE, 1, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff

#define I2C_Freq 100000

void CNFCHandler::setup( TwoWire *wire, int SDA_Pin, int SCL_Pin, int IRQ_Pin, int VEN_Pin ) 
{
  m_Wire = wire;
  if(!m_Wire->begin(SDA_Pin, SCL_Pin, I2C_Freq)) //starting I2C Wire
  {
    Serial.println("I2C Wire Error. Going idle.");
  }

  m_NFC = new Electroniccats_PN7150(IRQ_Pin, VEN_Pin, PN7150_ADDR, m_Wire);  // creates a global NFC device interface object, attached to pins 7 (IRQ) and 8 (VEN) and using the default I2C address 0x28

  Serial.println("Read ISO14443-3A(T2T) data block 5 with PN7150");

  Serial.println("Initializing...");
  if (m_NFC->connectNCI()) {  // Wake up the board
    Serial.println("Error while setting up the mode, check connections!");
  }

  if (m_NFC->configureSettings()) {
    Serial.println("The Configure Settings failed!");
  }

  if (m_NFC->configMode()) {  // Set up the configuration mode
    Serial.println("The Configure Mode failed!!");
  }
  m_NFC->startDiscovery();  // NCI Discovery mode
  Serial.println("Waiting for an ISO14443-3A Card...");
}

void CNFCHandler::loop()
{
  if( m_NFC->isTagDetected() )
  {
    Serial.println(m_NFC->remoteDevice.getProtocol());
    Serial.println(m_NFC->remoteDevice.getModeTech());
    Serial.println(m_NFC->remoteDevice.getInterface());
    
    menu TempMenu;
    TempMenu.iUserID = 1; // User id number 1
    TempMenu.iMenuLen = 3; // amount of menu items
    TempMenu.iaMenu[0] = 1; // Bread
    TempMenu.iaMenu[1] = 56; // Sausage
    TempMenu.iaMenu[2] = 80; // Juice
    
    WriteMenu(TempMenu);

    menu out;
    ReadMenu(out);

    out.Print();

    Serial.println("Remove the Card");
    m_NFC->waitForTagRemoval();
    Serial.println("CARD REMOVED!");
  }

  Serial.println("Restarting...");
  m_NFC->reset();
  Serial.println("Waiting for an ISO14443-3A Card...");
}

bool CNFCHandler::CheckCard(bool bWait)
{
  return m_NFC->isTagDetected();
}

void CNFCHandler::WaitForRemoval()
{
  m_NFC->waitForTagRemoval();
}

void CNFCHandler::Reset()
{
  m_NFC->reset();
}

int CNFCHandler::WriteMenu(menu newMenu)
{
  Serial.println("Start reading process...");
  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  /* Authenticate sector 1 with generic keys */
  unsigned char Auth[] = {0x40, BLK_NB_MFC / 4, 0x10, KEY_MFC};
  /* Write block 4 */
  unsigned char WritePart1[] = {0x10, 0xA0, BLK_NB_MFC};
  unsigned char WriteDataStart[] = {0x10, AUTH_CODE};
  unsigned char WritePart2[17];
  memcpy(WritePart2, WriteDataStart, sizeof(WriteDataStart));

  int iWriteStart = 3; // sizeof(WriteDataStart), 0x10, 'o', 'p'
  WritePart2[iWriteStart] = newMenu.iUserID;
  iWriteStart++;
  WritePart2[iWriteStart] = newMenu.iMenuLen;
  iWriteStart++;

  for( int i = 0; i < newMenu.iMenuLen; i++ )
  {
    WritePart2[iWriteStart] = newMenu.iaMenu[i];
    iWriteStart++;
  }

  // Clear the rest
  for( iWriteStart; iWriteStart < 17; iWriteStart++ )
  {
    WritePart2[iWriteStart] = 0;
  }

  /* Authenticate */
  status = m_NFC->readerTagCmd(Auth, sizeof(Auth), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    Serial.println("Auth error!");
    return 1;
  }

  /* Write block */
  status = m_NFC->readerTagCmd(WritePart1, sizeof(WritePart1), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    Serial.print("Error writing block!");
    return 3;
  }
  status = m_NFC->readerTagCmd(WritePart2, sizeof(WritePart2), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    Serial.print("Error writing block!");
    return 4;
  }

  return 0;
}

int CNFCHandler::ReadMenu(menu &out)
{
  Serial.println("Start reading process...");
  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  /* Authenticate sector 1 with generic keys */
  unsigned char Auth[] = {0x40, BLK_NB_MFC / 4, 0x10, KEY_MFC};
  /* Read block 4 */
  unsigned char Read[] = {0x10, 0x30, BLK_NB_MFC};

  /* Authenticate */
  status = m_NFC->readerTagCmd(Auth, sizeof(Auth), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    Serial.println("Auth error!");
    return 1;
  }

  /* Read block again to see te changes*/
  status = m_NFC->readerTagCmd(Read, sizeof(Read), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    Serial.print("Error reading block!");
    return 2;
  }

  unsigned char *data = Resp + 1;
  int numBytes = RespSize - 2;

  if( data[0] != 'o' && data[1] != 'p' )
  {
    Serial.println("KARTICA NIJE ISPRAVNO AUTENTIFICIRANA!");
    return 3;
  }

  data += 2;
  numBytes-= 2;

  out.iUserID = data[0];
  data++; numBytes--;
  out.iMenuLen = data[0];
  data++; numBytes--;
  
  uint32_t iPos;
  for( iPos = 0; iPos < numBytes; iPos++ )
  {
    out.iaMenu[iPos] = data[iPos];
  }

  return 0;
}