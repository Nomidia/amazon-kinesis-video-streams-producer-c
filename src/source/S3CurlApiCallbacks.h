/*******************************************
CURL API callbacks internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_S3_CURL_API_CALLBACKS_INCLUDE_I__
#define __KINESIS_VIDEO_S3_CURL_API_CALLBACKS_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#define S3_CA_CERT_PEM_FILE_SUFFIX ".pem"

#define S3_REQUEST_CONNECTION_TIMEOUT (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define S3_REQUEST_COMPLETION_TIMEOUT SERVICE_CALL_INFINITE_TIMEOUT
#define S3_IOT_CREDENTIAL_FETCH_GRACE_PERIOD                                                                                                         \
    (5 * HUNDREDS_OF_NANOS_IN_A_SECOND + MIN_STREAMING_TOKEN_EXPIRATION_DURATION + STREAMING_TOKEN_EXPIRATION_GRACE_PERIOD)

#define S3_SERVICE_NAME "s3"

typedef STATUS (*S3BlockingServiceCallFunc)(PRequestInfo, PCallInfo);

typedef struct __S3IotCredentialProvider {
    // First member should be the abstract credential provider
    AwsCredentialProvider credentialProvider;

    // Current time functionality - optional
    GetCurrentTimeFunc getCurrentTimeFn;

    // Custom data supplied to time function
    UINT64 customData;

    // S3 url
    CHAR s3Url[MAX_URI_CHAR_LEN + 1];

    // S3 file path
    CHAR filePath[MAX_PATH_LEN + 1];

    // CA certificate file path
    CHAR caCertPath[MAX_PATH_LEN + 1];

    // Region name
    CHAR region[MAX_REGION_NAME_LEN + 1];

    // User agent
    CHAR userAgent[MAX_USER_AGENT_LEN + 1];

    // String name is used as IoT thing-name
    CHAR thingName[MAX_STREAM_NAME_LEN + 1];

    // Static Aws Credentials structure with the pointer following the main allocation
    PAwsCredentials pAwsCredentials;

    // Service call functionality
    S3BlockingServiceCallFunc serviceCallFn;
} S3IotCredentialProvider, *PS3IotCredentialProvider;

STATUS blockingCurlCallForS3(PRequestInfo, PCallInfo, S3CurlCallbackFunc, PUINT32 pCallResult);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_S3_CURL_API_CALLBACKS_INCLUDE_I__ */
