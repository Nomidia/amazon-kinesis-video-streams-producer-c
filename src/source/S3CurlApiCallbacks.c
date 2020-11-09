/**
 * S3 CURL based API Callbacks
 */
#define LOG_CLASS "S3CurlApiCallbacks"
#include "Include_i.h"

// Forward declaration
STATUS createCurlHeaderList(PRequestInfo pRequestInfo, struct curl_slist** ppHeaderList);

STATUS blockingCurlCallForS3(PRequestInfo pRequestInfo, PCallInfo pCallInfo, S3CurlCallbackFunc writeFn)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CURL* curl = NULL;
    CURLcode res;
    PCHAR url;
    UINT32 httpStatusCode;
    struct curl_slist* pHeaderList = NULL;
    CHAR errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';
    UINT32 length;
    STAT_STRUCT entryStat;
    BOOL secureConnection;

    CHK(pRequestInfo != NULL && pCallInfo != NULL, STATUS_NULL_ARG);

    // CURL global initialization
    CHK(0 == curl_global_init(CURL_GLOBAL_ALL), STATUS_CURL_LIBRARY_INIT_FAILED);
    curl = curl_easy_init();
    CHK(curl != NULL, STATUS_CURL_INIT_FAILED);

    CHK_STATUS(createCurlHeaderList(pRequestInfo, &pHeaderList));

    // set verification for SSL connections
    CHK_STATUS(requestRequiresSecureConnection(pRequestInfo->url, &secureConnection));
    if (secureConnection) {
        // Use the default cert store at /etc/ssl in most common platforms
        if (pRequestInfo->certPath[0] != '\0') {
            CHK(0 == FSTAT(pRequestInfo->certPath, &entryStat), STATUS_DIRECTORY_ENTRY_STAT_ERROR);

            if (S_ISDIR(entryStat.st_mode)) {
                // Assume it's the path as we have a directory
                curl_easy_setopt(curl, CURLOPT_CAPATH, pRequestInfo->certPath);
            } else {
                // We should check for the extension being PEM
                length = (UINT32) STRNLEN(pRequestInfo->certPath, MAX_PATH_LEN);
                CHK(length > ARRAY_SIZE(S3_CA_CERT_PEM_FILE_SUFFIX), STATUS_INVALID_ARG_LEN);
                CHK(0 == STRCMPI(S3_CA_CERT_PEM_FILE_SUFFIX, &pRequestInfo->certPath[length - ARRAY_SIZE(S3_CA_CERT_PEM_FILE_SUFFIX) + 1]),
                    STATUS_INVALID_CA_CERT_PATH);
                curl_easy_setopt(curl, CURLOPT_CAINFO, pRequestInfo->certPath);
            }
        }

        // Enforce the public cert verification - even though this is the default
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pHeaderList);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_URL, pRequestInfo->url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, pCallInfo);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, pRequestInfo->connectionTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    if (pRequestInfo->completionTimeout != SERVICE_CALL_INFINITE_TIMEOUT) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, pRequestInfo->completionTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
    // Setting up limits for curl timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, pRequestInfo->lowSpeedTimeLimit / HUNDREDS_OF_NANOS_IN_A_SECOND);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, pRequestInfo->lowSpeedLimit);

    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    curl_easy_setopt(curl, CURLOPT_SSLCERT, NULL);
    curl_easy_setopt(curl, CURLOPT_SSLKEY, NULL);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);
        DLOGE("Curl perform failed for url %s with result %s : %s ", url, curl_easy_strerror(res), errorBuffer);
        CHK(FALSE, STATUS_IOT_FAILED);
    }
    double downloadFileLenth;
    res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &downloadFileLenth);
    if (!res) {
        PRINTF("response content-length:%f\n", downloadFileLenth);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatusCode);
    CHK_ERR(httpStatusCode == HTTP_STATUS_CODE_OK, STATUS_IOT_FAILED, "Curl call response failed with http status %lu", httpStatusCode);

CleanUp:

    if (pHeaderList != NULL) {
        curl_slist_free_all(pHeaderList);
    }

    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }

    LEAVES();
    return retStatus;
}

STATUS iotCurlHandlerForS3(PCHAR url, PCHAR region, PCHAR certPath, PAwsCredentialProvider pCredentialProvider, S3CurlCallbackFunc writeFn)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRequestInfo pRequestInfo = NULL;
    CallInfo callInfo;
    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));

    // Refresh the credentials
    UINT64 currentTime = GETTIME();

    PAwsCredentials pAwsCredentials;
    CHK_STATUS(pCredentialProvider->getCredentialsFn(pCredentialProvider, &pAwsCredentials));
    // PRINTF("accessKeyId:%s\n", pAwsCredentials->accessKeyId);
    // PRINTF("secretKey:%s\n", pAwsCredentials->secretKey);

    CHK(pAwsCredentials && currentTime + S3_IOT_CREDENTIAL_FETCH_GRACE_PERIOD < pAwsCredentials->expiration, retStatus);

    CHAR userAgent[MAX_USER_AGENT_LEN];
    retStatus = getUserAgentString(NULL, NULL, MAX_USER_AGENT_LEN, userAgent);
    userAgent[MAX_USER_AGENT_LEN] = '\0';
    if (STATUS_FAILED(retStatus)) {
        DLOGW("Failed to generate user agent string with error 0x%08x.", retStatus);
    }

    // Form a new request info based on the params
    CHK_STATUS(createRequestInfo(url, NULL, region, certPath, NULL, NULL, SSL_CERTIFICATE_TYPE_PEM, "Qualvision", S3_REQUEST_CONNECTION_TIMEOUT,
                                 S3_REQUEST_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pAwsCredentials,
                                 &pRequestInfo));

    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;
    STRNCPY(pRequestInfo->service, S3_SERVICE_NAME, MAX_SERVICE_NAME_LEN);

    callInfo.pRequestInfo = pRequestInfo;
    PRINTF("s3 signAwsRequestInfo, region[%s], certpath[%s]\n", pRequestInfo->region, pRequestInfo->certPath);
    CHK_STATUS(signAwsRequestInfo(pRequestInfo));
    // Perform a blocking call
    CHK_STATUS(blockingCurlCallForS3(pRequestInfo, &callInfo, writeFn));

CleanUp:

    if (pRequestInfo != NULL) {
        freeRequestInfo(&pRequestInfo);
    }

    releaseCallInfo(&callInfo);

    return retStatus;
}
