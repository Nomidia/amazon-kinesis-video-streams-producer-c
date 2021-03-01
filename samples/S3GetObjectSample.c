#include <com/amazonaws/kinesis/video/cproducer/Include.h>

#define FILE_PATH   "/qa/Firmwares/CameraVendor/CameraCategory/ProductPTZ/Beta/60015/Beta_12_60015.bin"
#define BUCKET_NAME "ehss-private-bucket"
#define URL_FIX     ".s3.ap-south-1.amazonaws.com"
//#define HOST BUCKET_NAME URL_FIX
#define S3_DEBUG 1

BOOL s3CallResultRetry(SERVICE_CALL_RESULT callResult);

SIZE_T writeCurlResponseCallbackForS3(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData);

BOOL s3CallResultRetry(SERVICE_CALL_RESULT callResult)
{
    switch (callResult) {
        case SERVICE_CALL_INVALID_ARG:
        case SERVICE_CALL_NOT_AUTHORIZED:
        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_RESOURCE_DELETED:
            return FALSE;

        case SERVICE_CALL_RESOURCE_NOT_FOUND:
        case SERVICE_CALL_REQUEST_TIMEOUT:
        case SERVICE_CALL_GATEWAY_TIMEOUT:
        case SERVICE_CALL_NETWORK_READ_TIMEOUT:
        case SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT:
        case SERVICE_CALL_RESULT_OK:
        case SERVICE_CALL_UNKNOWN:
            // Explicit fall-through
        default:
            // Unknown error
            return TRUE;
    }
}

SIZE_T writeCurlResponseCallbackForS3(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData)
{
    PCallInfo pCallInfo = (PCallInfo) customData;

    // Does not include the NULL terminator
    SIZE_T dataSize = size * numItems;

    if (pCallInfo == NULL) {
        // return CURL_READFUNC_ABORT;
        return dataSize;
    }

    pCallInfo->responseDataLen += (UINT32) dataSize;

    PRINTF("dataSize = %d, total responseDataLen = %d\n", dataSize, pCallInfo->responseDataLen);
    // return CURL_READFUNC_ABORT;

    return dataSize;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    if (argc < 2) {
        PRINTF("Wrong number of parameters\n");
        return -1;
    }
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 callResult = SERVICE_CALL_RESULT_OK;
    PCHAR streamName = NULL, region = NULL, cacertPath = NULL;
    SET_INSTRUMENTED_ALLOCATORS();

    PAwsCredentialProvider pCredentialProvider = NULL;

    streamName = argv[1];
    if ((cacertPath = getenv(CACERT_PATH_ENV_VAR)) == NULL) {
        PRINTF("Need certPath\n");
        return -1;
    }
    if ((region = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        PRINTF("Need region\n");
        return -1;
    }

    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_DEBUG);

#if S3_DEBUG
    CHK_STATUS(createCurlIotCredentialProvider("c3qp4tl980s52m.credentials.iot.ap-south-1.amazonaws.com",
                                               "/mnt/mtd/Config/iotcerts/deviceCertificate.crt", "/mnt/mtd/Config/iotcerts/deviceCertificate.key",
                                               "/usr/data/certs/AmazonRootCA1.pem", "dev-kvs-access-role-alias", streamName, &pCredentialProvider));

#endif
    // Create url
    CHAR s3Url[MAX_URI_CHAR_LEN + 1];
    memset(s3Url, 0, sizeof(s3Url));

    SNPRINTF(s3Url, MAX_URI_CHAR_LEN, "%s%s%s%s", CONTROL_PLANE_URI_PREFIX, BUCKET_NAME, URL_FIX, FILE_PATH);

    // PRINTF("url:%s\n", s3Url);

#if S3_DEBUG
    retStatus = iotCurlHandlerForS3(s3Url, region, cacertPath, pCredentialProvider, writeCurlResponseCallbackForS3, &callResult);
#endif
CleanUp:
    printf("callResult = %d\n", callResult);
    if (STATUS_FAILED(retStatus)) {
        defaultLogPrint(LOG_LEVEL_ERROR, "", "Failed with status 0x%08x\n", retStatus);

        if(serviceCallResultRetry((SERVICE_CALL_RESULT)callResult)) {
            defaultLogPrint(LOG_LEVEL_DEBUG, "", "retryable\n");
        }
    }

#if S3_DEBUG
    freeIotCredentialProvider(&pCredentialProvider);
#endif

    RESET_INSTRUMENTED_ALLOCATORS();

    return (INT32) retStatus;
}
