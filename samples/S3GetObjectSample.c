#include <com/amazonaws/kinesis/video/cproducer/Include.h>

#define FILE_PATH   "/qa/Firmwares/CameraVendor/CameraCategory/ProductPTZ/Beta/60015/Beta_12_60015.bin"
#define BUCKET_NAME "ehss-private-bucket"
#define URL_FIX     ".s3.ap-south-1.amazonaws.com"
//#define HOST BUCKET_NAME URL_FIX
#define S3_DEBUG 1

#define S3_DOWNLOAD_SUCCESS 0
#define S3_DOWNLOAD_FAIL -1
#define S3_DOWNLOAD_RETRYABLE -2
#define S3_DOWNLOAD_IDLE -3

BOOL s3CallResultRetry(SERVICE_CALL_RESULT callResult);

SIZE_T writeCurlFirmwareData(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData);
SIZE_T writeCurlPublicKey(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData);

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

SIZE_T writeCurlFirmwareData(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData)
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

SIZE_T writeCurlPublicKey(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData)
{
    PCallInfo pCallInfo = (PCallInfo) customData;

    // Does not include the NULL terminator
    SIZE_T dataSize = size * numItems;

    if (pCallInfo == NULL) {
        // return CURL_READFUNC_ABORT;
        return dataSize;
    }

    pCallInfo->responseDataLen += (UINT32) dataSize;

    PRINTF("%.*s\n", dataSize, pCallInfo->responseDataLen);
    // return CURL_READFUNC_ABORT;

    return dataSize;
}

int s3PerformCurl(PCHAR streamName, PCHAR url, PCHAR region, PCHAR cacertPath, S3CurlCallbackFunc writeCallback)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 callResult = SERVICE_CALL_RESULT_OK;
    PAwsCredentialProvider pCredentialProvider = NULL;
    int ret = S3_DOWNLOAD_SUCCESS;

#if S3_DEBUG
    CHK_STATUS(createCurlIotCredentialProvider("c3qp4tl980s52m.credentials.iot.ap-south-1.amazonaws.com",
                                               "/mnt/mtd/Config/iotcerts/deviceCertificate.crt", "/mnt/mtd/Config/iotcerts/deviceCertificate.key",
                                               "/usr/data/certs/AmazonRootCA1.pem", "dev-kvs-access-role-alias", streamName, &pCredentialProvider));

#endif

    retStatus = iotCurlHandlerForS3(url, region, cacertPath, pCredentialProvider, writeCallback, &callResult);

CleanUp:
    if (STATUS_FAILED(retStatus))
    {
        printf("AwsS3Client Failed with status 0x%08x\n", retStatus);

        ret = S3_DOWNLOAD_FAIL;
        if(s3CallResultRetry((SERVICE_CALL_RESULT)callResult))
        {
            printf("retryable\n");
            ret = S3_DOWNLOAD_RETRYABLE;
        }
    }

    freeIotCredentialProvider(&pCredentialProvider);

    return ret;
}

int s3DownloadFirmware(PCHAR streamName, PCHAR url, PCHAR region, PCHAR cacertPath)
{
    return s3PerformCurl(streamName, url, region, cacertPath, writeCurlFirmwareData);
}

int s3DownloadPublicKey(PCHAR streamName, PCHAR url, PCHAR region, PCHAR cacertPath)
{
    return s3PerformCurl(streamName, url, region, cacertPath, writeCurlPublicKey);
}

INT32 main(INT32 argc, CHAR* argv[])
{
    if (argc < 2) {
        PRINTF("Wrong number of parameters\n");
        return -1;
    }
    int ret = 0;
    PCHAR streamName = NULL, region = NULL, cacertPath = NULL;
    SET_INSTRUMENTED_ALLOCATORS();

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

    // Create url
    CHAR s3Url[MAX_URI_CHAR_LEN + 1];
    memset(s3Url, 0, sizeof(s3Url));

    SNPRINTF(s3Url, MAX_URI_CHAR_LEN, "%s%s%s%s", CONTROL_PLANE_URI_PREFIX, BUCKET_NAME, URL_FIX, FILE_PATH);

    // PRINTF("url:%s\n", s3Url);

#if S3_DEBUG
    ret = s3DownloadFirmware(streamName, s3Url, region, cacertPath);
#endif
    printf("ret = %d\n", ret);


    RESET_INSTRUMENTED_ALLOCATORS();

    return ret;
}
