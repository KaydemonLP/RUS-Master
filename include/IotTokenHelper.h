#pragma once

extern az_span generate_sas_token(
    az_iot_hub_client* hub_client,
    az_span device_key,
    az_span sas_signature,
    unsigned int expiryTimeInMinutes,
    az_span sas_token);

extern uint32_t getSasTokenExpiration(const char* sasToken);