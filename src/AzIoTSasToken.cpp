// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT
#define hehehoho
#include "iothub.h"
#include <stdlib.h>
#include <time.h>
#include <IotTokenHelper.h>

#define INDEFINITE_TIME ((time_t)-1)

AzIoTSasToken::AzIoTSasToken(
    az_iot_hub_client* client,
    az_span deviceKey,
    az_span signatureBuffer,
    az_span sasTokenBuffer)
{
  this->client = client;
  this->deviceKey = deviceKey;
  this->signatureBuffer = signatureBuffer;
  this->sasTokenBuffer = sasTokenBuffer;
  this->expirationUnixTime = 0;
  this->sasToken = AZ_SPAN_EMPTY;
}

int AzIoTSasToken::Generate(unsigned int expiryTimeInMinutes)
{
  this->sasToken = generate_sas_token(
      this->client,
      this->deviceKey,
      this->signatureBuffer,
      expiryTimeInMinutes,
      this->sasTokenBuffer);

  if (az_span_is_content_equal(this->sasToken, AZ_SPAN_EMPTY))
  {
    //Logger.Error("Failed generating SAS token");
    return 1;
  }
  else
  {
    this->expirationUnixTime = getSasTokenExpiration((const char*)az_span_ptr(this->sasToken));

    if (this->expirationUnixTime == 0)
    {
      //Logger.Error("Failed getting the SAS token expiration time");
      this->sasToken = AZ_SPAN_EMPTY;
      return 1;
    }
    else
    {
      return 0;
    }
  }
}

bool AzIoTSasToken::IsExpired()
{
  time_t now = time(NULL);

  if (now == INDEFINITE_TIME)
  {
    //Logger.Error("Failed getting current time");
    return true;
  }
  else
  {
    return (now >= this->expirationUnixTime);
  }
}

az_span AzIoTSasToken::Get() { return this->sasToken; }